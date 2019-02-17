#!/usr/bin/env nodejs

/*

This script generates markdown files for libdill documentation.
These are further processed to generate both UNIX and HTML man pages.

Schema:

    name: name of the function
    section: the name of the section the function belongs to
    info: short description of the function
    is_in_libdillimpl: if true, the function is in libdillimpl.h
    protocol : { // should be present only if the function is related
                 // to a network protocol
        name: name of the protocol
        info: description of the protocol
        example: example of usage of the protocol, a piece of C code
    }
    result: { // omit this field for void functions
        type: return type of the function
    }
    args: [ // may be ommited for functions with no arguments
        {
            name: name of the argument
            type: type of the argument
            suffix: part of the type to go after the name (e.g. "[]")
            info: description of the argument
        }
    ]
    add_to_synopsis: a piece of code to be added to synopsis, between the
                     include and the function declaration
    has_iol: if true, adds I/O list to the list of arguments
    has_deadline: if true, adds deadline to the list of arguments
    has_handle_argument: set to true if at least one of the arguments is
                         a handle
    allocates_handle: set to true is the function allocates at least one handle
    prologue: the part of the description of the function to go before the list
              of arguments
    epilogue: the part of the description of the function to go after the list
              of arguments
    errors: a list of possible errors, the descriptions will be pulled from
            standard_errors object
    custom_errors: [{ // custom errors take precedence over standard errors
        error name: error description
    }]
    example: example of the usage of the function, a piece of C code;
             if present, overrides the protocol example
    mem: size // if set, _mem variant of the function will be generated;
              // size if the name of the storage struct, e.h. "tcp_storage".
*/

var fs = require('fs');

standard_errors = {
    EBADF: "Invalid handle.",
    EBUSY: "The handle is currently being used by a different coroutine.",
    ETIMEDOUT: "Deadline was reached.",
    ENOMEM: "Not enough memory.",
    EMFILE: "The maximum number of file descriptors in the process are " +
            "already open.",
    ENFILE: "The maximum number of file descriptors in the system are " +
            "already open.",
    EINVAL: "Invalid argument.",
    EMSGSIZE: "The data won't fit into the supplied buffer.",
    ECONNRESET: "Broken connection.",
    ECANCELED: "Current coroutine was canceled.",
    ENOTSUP: "The handle does not support this operation.",
}

bundle_example = `
    int b = bundle();
    bundle_go(b, worker());
    bundle_go(b, worker());
    bundle_go(b, worker());
    /* Give wrokers 1 second to finish. */
    bundle_wait(b, now() + 1000);
    /* Cancel any remaining workers. */
    hclose(b);
`,

ipaddr_example = `
    ipaddr addr;
    ipaddr_remote(&addr, "www.example.org", 80, 0, -1);
    int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
    connect(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
`

ipaddr_mode = `
    Mode specifies which kind of addresses should be returned. Possible
    values are:

    * **IPADDR_IPV4**: Get IPv4 address.
    * **IPADDR_IPV6**: Get IPv6 address.
    * **IPADDR_PREF_IPV4**: Get IPv4 address if possible, IPv6 address otherwise.
    * **IPADDR_PREF_IPV6**: Get IPv6 address if possible, IPv4 address otherwise.

    Setting the argument to zero invokes default behaviour, which, at the
    present, is **IPADDR_PREF_IPV4**. However, in the future when IPv6 becomes
    more common it may be switched to **IPADDR_PREF_IPV6**.
`

go_info = `
    The coroutine is executed concurrently, and its lifetime may exceed the
    lifetime of the caller coroutine. The return value of the coroutine, if any,
    is discarded and cannot be retrieved by the caller.

    Any function to be invoked as a coroutine must be declared with the
    **coroutine** specifier.

    Use **hclose** to cancel the coroutine. When the coroutine is canceled
    all the blocking calls within the coroutine will start failing with
    **ECANCELED** error.

    _WARNING_: Coroutines will most likely work even without the coroutine
    specifier. However, they may fail in random non-deterministic ways,
    depending on the code in question and the particular combination of compiler
    and optimization level. Additionally, arguments to a coroutine must not be
    function calls. If they are, the program may fail non-deterministically.
    If you need to pass a result of a computation to a coroutine, do the
    computation first, and then pass the result as an argument.  Instead of:

    \`\`\`c
    go(bar(foo(a)));
    \`\`\`

    Do this:

    \`\`\`c
    int a = foo();
    go(bar(a));
    \`\`\`
`

ws_flags = `
            The socket can be either text- (**WS_TEXT** flag) or binary-
            (**WS_BINARY** flag) based. Binary is the default. When sending
            messages via **msend** or **msendl** these will be typed based on
            the socket type. When receiving messages via **mrecv** or **mrecvl**
            encountering a message that doesn't match the socket type results in
            **EPROTO** error.

            If you want to combine text and binary messages you can do so by
            using functions such as **ws_send** and **ws_recv**.

            **WS_NOHTTP** flag can be combined with socket type flags. If set,
            the protocol will skip the initial HTTP handshake. In this case
            **resource** and **host** arguments won't be used and can be set
            to **NULL**.

            Skipping HTTP handshake is useful when you want to do the handshake
            on your own. For example, if you want to implement custom WebSocket
            extensions or if you want to write a multi-protocol application
            where initial HTTP handshake can be followed by different kinds
            of protocols (e.g. HTML and WebSocket).
`

http_server_example = `
    int s = tcp_accept(listener, NULL, -1);
    s = http_attach(s, -1);
    char command[256];
    char resource[256];
    http_recvrequest(s, command, sizeof(command), resource, sizeof(resource), -1);
    while(1) {
        char name[256];
        char value[256];
        int rc = http_recvfield(s, name, sizeof(name), value, sizeof(value), -1);
        if(rc == -1 && errno == EPIPE) break;
    }
    http_sendstatus(s, 200, "OK", -1);
    s = http_detach(s, -1); 
    tcp_close(s);
`

// Add all topics.
var files = fs.readdirSync(".");
for(var i in files) {
    if(!files[i].endsWith(".topic.js")) continue
    eval(fs.readFileSync(files[i]).toString())
}

fxs = []
fxnames = {} 

function newFunction(f) {
    if(f.name in fxnames) throw "duplicate function name: " + f.name
    fxs.push(f)
    fxnames[f.name] = null
}

// Add all functions to fxs.
var files = fs.readdirSync(".");
for(var i in files) {
    if(!files[i].endsWith(".function.js")) continue
    eval(fs.readFileSync(files[i]).toString())
}

// Trims whitespeace around the rectangular area of text.
function trimrect(t) {
    // Trim empty lines at the top and the bottom.
    var lns = t.split("\n")
    var first = lns.findIndex(function(e) {return e.trim().length > 0})
    if(first < 0) first = 0
    var last = lns.slice().reverse().findIndex(function(e) {
        return e.trim().length > 0
    })
    if(last < 0) last = lns.length
    else last = lns.length - last
    var lines = lns.slice(first, last)

    // Determine minimal left indent.
    var indent = -1
    for(var i = 0; i < lines.length; i++) {
        if(lines[i].trim().length == 0) continue
        var n = lines[i].length - lines[i].trimLeft().length
        if(n < indent || indent == -1) indent = n
    }
    // Trim the whitespace from the left and the right.
    lns = []
    for(var i = 0; i < lines.length; i++) {
        lns.push(lines[i].substr(indent).trimRight())
    }

    return lns.join("\n")
}

// Generate man page for one function.
function generate_man_page(fx, sections, mem) {
    var t = "";

    /**************************************************************************/
    /*  NAME                                                                  */
    /**************************************************************************/
    var fxname = fx.name
    if(mem) fxname += "_mem"
    t += "# NAME\n\n"
    t += fxname
    t += " - " + fx.info + "\n\n"

    /**************************************************************************/
    /*  SYNOPSIS                                                              */
    /**************************************************************************/
    t += "# SYNOPSIS\n\n```c\n"
    if(fx.is_in_libdillimpl) {
        t += "#include <libdillimpl.h>\n\n"
    } else {
        t += "#include <libdill.h>\n\n"
    }
    if(fx.add_to_synopsis) {
        t += trimrect(fx.add_to_synopsis) + "\n\n"
    }
    if(fx.result)
        var rtype = fx.result.type
    else
        var rtype = "void"
    t += rtype + " " + fx.name
    if(mem) t += "_mem"
    t += "("
    if(fx.args) var a = fx.args.slice()
    else var a = []

    if(fx.has_iol) {
        a.push({
            name: "first",
            type: "struct iolist*",
            info: "Pointer to the first item of a linked list of I/O buffers.",
        })
        a.push({
            name: "last",
            type: "struct iolist*",
            info: "Pointer to the last item of a linked list of I/O buffers.",
        })
    }
    if(mem) {
        memarg = {
            name: "mem",
            type: "struct " + fx.mem + "*",
            info: "The structure to store the newly created object in. It " +
                  "must not be deallocated before the object is closed.",
        }
        if(fx.name === "chmake" || fx.name === "ipc_pair") a.unshift(memarg)
        else a.push(memarg)
    }
    if(fx.has_deadline) {
        a.push({
            name: "deadline",
            type: "int64_t",
            info: "A point in time when the operation should time out, in " +
                  "milliseconds. Use the **now** function to get your current" +
                  " point in time. 0 means immediate timeout, i.e., perform " +
                  "the operation if possible or return without blocking if " +
                  "not. -1 means no deadline, i.e., the call will block " +
                  "forever if the operation cannot be performed.",
        })
    }

    if(a.length == 0 && !fx.has_iol && !mem && !fx.has_deadline) {
        t += "void"
    } else {
        t += "\n"
        for(var j = 0; j < a.length; j++) {
            t += "    "
            if(a[j].type) t += a[j].type + " "
            t += a[j].name
            if(a[j].suffix) t += a[j].suffix
            if(j != a.length - 1) t += ",\n"
        }
    }
    t += ");\n```\n\n"

    /**************************************************************************/
    /*  DESCRIPTION                                                           */
    /**************************************************************************/
    t += "# DESCRIPTION\n\n"

    if(fx.experimental || (fx.protocol && fx.protocol.experimental)) {
        t += "**WARNING: This is experimental functionality and the API may " +
             "change in the future.**\n\n"
    }

    if(fx.protocol) {
        t += trimrect(fx.protocol.info) + "\n\n"
    }

    if(fx.prologue) {
        t += trimrect(fx.prologue) + "\n\n"
    }

    if(fx.has_iol) {
        t += trimrect(`
            This function accepts a linked list of I/O buffers instead of a
            single buffer. Argument **first** points to the first item in the
            list, **last** points to the last buffer in the list. The list
            represents a single, fragmented message, not a list of multiple
            messages. Structure **iolist** has the following members:

            \`\`\`c
            void *iol_base;          /* Pointer to the buffer. */
            size_t iol_len;          /* Size of the buffer. */
            struct iolist *iol_next; /* Next buffer in the list. */
            int iol_rsvd;            /* Reserved. Must be set to zero. */
            \`\`\`

            When receiving, **iol_base** equal to NULL means that **iol_len**
            bytes should be skipped.

            The function returns **EINVAL** error in the case the list is
            malformed:

            * If **last->iol_next** is not **NULL**.
            * If **first** and **last** don't belong to the same list.
            * If there's a loop in the list.
            * If **iol_rsvd** of any item is non-zero.

            The list (but not the buffers themselves) can be temporarily
            modified while the function is in progress. However, once the
            function returns the list is guaranteed to be the same as before
            the call.
        ` )+ "\n\n"
    }

    if(mem) {
        t += trimrect(`
            This function allows to avoid one dynamic memory allocation by
            storing the object in user-supplied memory. Unless you are
            hyper-optimizing use **` + fx.name + "** instead.") + "\n\n"
    }

    for(var j = 0; j < a.length; j++) {
        arg = a[j]
        t += "**" + arg.name + "**: " + arg.info + "\n\n"
    }

    if(fx.epilogue) {
        t += trimrect(fx.epilogue) + "\n\n"
    }

    if(fx.protocol || fx.section === "IP addresses") {
        t += "This function is not available if libdill is compiled with " +
             "**--disable-sockets** option.\n\n"
    }

    if(fx.protocol && fx.protocol.section === "TLS protocol") {
        t += "This function is not available if libdill is compiled " +
             "without **--enable-tls** option.\n\n"
    }

    /**************************************************************************/
    /*  RETURN VALUE                                                          */
    /**************************************************************************/
    t += "# RETURN VALUE\n\n"
    if(fx.result) {
        
        if(fx.result.success && fx.result.error) {
            t += "In case of success the function returns " +
                fx.result.success + ". "
            t += "In case of error it returns " + fx.result.error +
                " and sets **errno** to one of the values below.\n\n"
        }
        if(fx.result.info) {
            t += trimrect(fx.result.info) + "\n\n"
        }
    } else {
        t += "None.\n\n"
    }

    /**************************************************************************/
    /*  ERRORS                                                                */
    /**************************************************************************/
    t += "# ERRORS\n\n"
    if(fx.errors) {
        var errs = fx.errors.slice()
    }
    else {
        var errs = []
    }

    if(fx.has_handle_argument) errs.push("EBADF", "ENOTSUP")
    if(fx.has_deadline) {
        errs.push("ECANCELED")
        if(fx.name != "msleep") errs.push("ETIMEDOUT")
    }
    if(fx.allocates_handle) errs.push("EMFILE", "ENFILE", "ENOMEM")
    if(fx.uses_connection) errs.push("ECONNRESET", "ECANCELED")
    if(fx.mem && !mem) errs.push("ENOMEM")

    // Expand error descriptions.
    var errdict = {}
    for(var idx = 0; idx < errs.length; idx++) {
        errdict[errs[idx]] = standard_errors[errs[idx]]
    }

    // Add custom errors.
    if(fx.custom_errors) Object.assign(errdict, fx.custom_errors)

    // Print errors in alphabetic order.
    var codes = Object.keys(errdict).sort()
    if(codes.length == 0) {
        t += "None.\n"
    }
    else {
        for(var j = 0; j < codes.length; j++) {
            t += "* **" + codes[j] + "**: " + errdict[codes[j]] + "\n"
        }
    }
    t += "\n"

    if(fx.add_to_errors) t += trimrect(fx.add_to_errors) + "\n\n"

    /**************************************************************************/
    /*  EXAMPLE                                                               */
    /**************************************************************************/
    if(fx.example || (fx.protocol && fx.protocol.example)) {
        t += "# EXAMPLE\n\n"
        var example = fx.example
        if(example == undefined) example = fx.protocol.example
        t += "```c\n" + trimrect(example) + "\n```\n"
    }

    /**************************************************************************/
    /*  SEE ALSO                                                              */
    /**************************************************************************/
    t += "# SEE ALSO\n\n"
    if(fx.section) var section = fx.section
    else if(fx.protocol) var section = fx.protocol.section
    else section = "Unclassified"
    seealso = sections[section].slice()
    /* No need to have reference to itself. */
    var i = seealso.indexOf(fxname);
    seealso.splice(i, 1);
    /* Custom see also items. */
    if(fx.has_deadline) seealso.push("now")
    if(fx.allocates_handle) seealso.push("hclose")
    if(fx.protocol && fx.protocol.type === "bytestream") {
        seealso.push("brecv")
        seealso.push("brecvl")
        seealso.push("bsend")
        seealso.push("bsendl")
    }
    if(fx.protocol && fx.protocol.type === "message") {
        seealso.push("mrecv")
        seealso.push("mrecvl")
        seealso.push("msend")
        seealso.push("msendl")
    }
    seealso.sort()
    for(var i = 0; i < seealso.length; i++) {
        t += "**" + seealso[i] + "**(3) "
    }
    t += "\n"

    return t
}

function generate_section(name, sections) {
    var t = "#### " + name + "\n\n"
    section = sections[name]
    section.sort()
    for(var i = 0; i < section.length; i++) {
        t += "* [" + section[i] + "(3)](" + section[i] +".html)\n"
    }
    t += "\n"
    return t
}

// Generate ToC.
var sections = {}
for(var i = 0; i < fxs.length; i++) {
      fx = fxs[i];
    if(fx.section) var section = fx.section
    else if(fx.protocol) var section = fx.protocol.section
    else section = "Unclassified"
    if(!sections[section]) sections[section] = []
    sections[section].push(fx.name)
    if(fx.mem) sections[section].push(fx.name + "_mem")
}
t = ""
t += generate_section("Coroutines", sections)
t += generate_section("Deadlines", sections)
t += generate_section("Channels", sections)
t += generate_section("Handles", sections)
t += generate_section("File descriptors", sections)
t += generate_section("Bytestream sockets", sections)
t += generate_section("Message sockets", sections)
t += generate_section("IP addresses", sections)
t += generate_section("Happy Eyeballs protocol", sections)
t += generate_section("HTTP protocol", sections)
t += generate_section("IPC protocol", sections)
t += generate_section("PREFIX protocol", sections)
t += generate_section("SUFFIX protocol", sections)
t += generate_section("TCP protocol", sections)
t += generate_section("TERM protocol", sections)
t += generate_section("TLS protocol", sections)
t += generate_section("UDP protocol", sections)
t += generate_section("WebSocket protocol", sections)
fs.writeFileSync("toc.md", t)

// Subsequent lines without an empty line between them should be joined
// to form a single paragraph.
function make_paragraphs(t) {
    var lns = t.split("\n")
    var res = ""
    var prev = "empty"
    for(var i = 0; i < lns.length; i++) {
        var ln = lns[i]
        if(prev === "quotes") {
            if(ln === "```") {
                res += "```\n\n"
                prev = "empty"
                continue
            }
            res += ln + "\n"
            continue
        }
        if(ln.length == 0) {
            if(prev === "empty") continue
            if(prev === "list") res += "\n"
            else res += "\n\n"
            prev = "empty"
            continue
        }
        if(ln.startsWith("#")) {
            if(prev !== "empty") res += "\n\n"
            res += ln + "\n\n"
            prev = "empty"
            continue
        }
        if(ln.startsWith("* ")) {
            res += ln + "\n"
            prev = "list"
            continue
        }
        if(ln.startsWith("```")) {
            if(prev !== "empty") res += "\n\n"
            res += ln + "\n"
            prev = "quotes"
            continue
        }
        res += " " + ln
        prev = "text"
    }
    return res
}

// Generate individual man pages.
for(var i = 0; i < fxs.length; i++) {
    fx = fxs[i];
    t = generate_man_page(fx, sections, false)
    t = make_paragraphs(t)
    fs.writeFileSync(fx.name + ".md", t)
    // Mem functions have no definitions of their own. The docs are generated
    // from the related non-mem function.
    if(fx.mem != undefined) {
        t = generate_man_page(fx, sections, true)
        t = make_paragraphs(t)
        fs.writeFileSync(fx.name + "_mem.md", t)
    }
 }

