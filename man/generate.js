#!/usr/bin/env nodejs

var fs = require('fs');

crlf_protocol = {
    name: "CRLF",
    info: "CRLF is a message-based protocol that delimits messages usign CR+LF byte sequence (0x0D 0x0A). In other words, it's a protocol to send text messages separated by newlines. The protocol has no initial handshake. Terminal handshake is accomplished by each peer sending an empty line.",
    example: `
int s = tcp_connect(&addr, -1);
s = crlf_attach(s);
msend(s, "ABC", 3, -1);
char buf[256];
ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
s = crlf_detach(s, -1);
tcp_close(s);
`
}

pfx_protocol = {
    name: "PFX",
    info: "PFX  is a message-based protocol to send binary messages prefixed by 8-byte size in network byte order. The protocol has no initial handshake. Terminal handshake is accomplished by each peer sending eight 0xFF bytes.",
    example: `
int s = tcp_connect(&addr, -1);
s = pfx_attach(s);
msend(s, "ABC", 3, -1);
char buf[256];
ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
s = pfx_detach(s, -1);
tcp_close(s);
`
}

tls_protocol = {
    name: "TLS",
    info: "TLS is a cryptographic protocol to provide secure communication over the network. It is a bytestream protocol.",
    example: `
int s = tcp_connect(&addr, -1);
s = tls_attach_client(s, -1);
bsend(s, "ABC", 3, -1);
char buf[3];
ssize_t sz = brecv(s, buf, sizeof(buf), -1);
s = tls_detach(s, -1);
tcp_close(s);
`,
    experimental: true,
}


fxs = [
    {
        name: "crlf_attach",
        info: "creates CRLF protocol on top of underlying socket",
        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the underlying socket. It must be a bytestream protocol.",
           },
        ],
        protocol: crlf_protocol,
        prologue: "This function instantiates CRLF protocol on top of the underlying protocol.",
        epilogue: "The socket can be cleanly shut down using **crlf_detach** function.",

        has_handle_argument: true,
        has_deadline: false,
        allocates_memory: true,
        allocates_handle: true,
        sendsrecvs: false,
        mem: "CRLF_SIZE",

        errors: {
            EPROTO: "Underlying socket is not a bytestream socket.",
        },
    },
    {
        name: "crlf_detach",
        info: "terminates CRLF protocol and returns the underlying socket",
        result: {
            type: "int",
            success: "underlying socket handle",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the CRLF socket.",
           },
        ],
        protocol: crlf_protocol,
        prologue: "This function does the terminal handshake and returns underlying socket to the user. The socket is closed even in the case of error.",

        has_handle_argument: true,
        has_deadline: true,
        allocates_memory: false,
        allocates_handle: false,
        sendsrecvs: true,

        errors: {
            ENOTSUP: "The handle is not a CRLF protocol handle.",
        },
    },
    {
        name: "pfx_attach",
        info: "creates PFX protocol on top of underlying socket",
        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the underlying socket. It must be a bytestream protocol.",
           },
        ],
        protocol: pfx_protocol,
        prologue: "This function instantiates PFX protocol on top of the underlying protocol.",
        epilogue: "The socket can be cleanly shut down using **pfx_detach** function.",

        has_handle_argument: true,
        has_deadline: false,
        allocates_memory: true,
        allocates_handle: true,
        sendsrecvs: false,
        mem: "PFX_SIZE",

        errors: {
            EPROTO: "Underlying socket is not a bytestream socket.",
        },
    },
    {
        name: "pfx_detach",
        info: "terminates PFX protocol and returns the underlying socket",
        result: {
            type: "int",
            success: "underlying socket handle",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the PFX socket.",
           },
        ],
        protocol: pfx_protocol,
        prologue: "This function does the terminal handshake and returns underlying socket to the user. The socket is closed even in the case of error.",

        has_handle_argument: true,
        has_deadline: true,
        allocates_memory: false,
        allocates_handle: false,
        sendsrecvs: true,

        errors: {
            ENOTSUP: "The handle is not a PFX protocol handle.",
        },
    },
    {
        name: "tls_attach_client",
        info: "creates TLS protocol on top of underlying socket",
        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the underlying socket. It must be a bytestream protocol.",
           },
        ],
        protocol: tls_protocol,
        prologue: "This function instantiates TLS protocol on top of the underlying protocol. TLS protocol being asymmetric, client and server sides are intialized in different ways. This particular function initializes the client side of the connection.",
        epilogue: "The socket can be cleanly shut down using **tls_detach** function.",

        has_handle_argument: true,
        has_deadline: true,
        allocates_memory: true,
        allocates_handle: true,
        sendsrecvs: true,
        mem: "TLS_SIZE",

        errors: {
            EPROTO: "Underlying socket is not a bytestream socket.",
        },
    },
    {
        name: "tls_attach_server",
        info: "creates TLS protocol on top of underlying socket",
        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the underlying socket. It must be a bytestream protocol.",
           },
           {
               name: "cert",
               type: "const char*",
               info: "Filename of the file contianing the certificate.",
           },
           {
               name: "cert",
               type: "const char*",
               info: "Filename of the file contianing the private key.",
           },
        ],
        protocol: tls_protocol,
        prologue: "This function instantiates TLS protocol on top of the underlying protocol. TLS protocol being asymmetric, client and server sides are intialized in different ways. This particular function initializes the server side of the connection.",
        epilogue: "The socket can be cleanly shut down using **tls_detach** function.",

        has_handle_argument: true,
        has_deadline: true,
        allocates_memory: true,
        allocates_handle: true,
        sendsrecvs: true,
        mem: "TLS_SIZE",

        errors: {
            EPROTO: "Underlying socket is not a bytestream socket.",
        },
    },
]

function generate_man_page(fx, mem) {
    var t = "";

    /**************************************************************************/
    /*  NAME                                                                  */
    /**************************************************************************/
    t += "# NAME\n\n"
    t += fx.name
    if(mem) t += "_mem"
    t += " - " + fx.info + "\n\n"

    /**************************************************************************/
    /*  SYNOPSIS                                                              */
    /**************************************************************************/
    t += "# SYNOPSIS\n\n"
    t += "**#include &lt;libdill.h>**\n\n"
    t += "**" + fx.result.type + " " + fx.name
    if(mem) t += "_mem"
    t += "("
    for(var j = 0; j < fx.args.length; j++) {
        arg = fx.args[j]
        t += arg.type + " **_" + arg.name + "_**"
        if(j != fx.args.length - 1) t += ", "
    }
    if(mem) t += ", void **\*_mem_**"
    if(fx.has_deadline) t += ", int64_t **_deadline_**"
    t += ");**\n\n"

    /**************************************************************************/
    /*  DESCRIPTION                                                           */
    /**************************************************************************/
    t += "# DESCRIPTION\n\n"

    if(fx.experimental || (fx.protocol && fx.protocol.experimental)) {
        t += "**WARNING: This is experimental functionality and the API may change in the future.**\n\n"
    }

    if(fx.protocol != undefined) {
        t += fx.protocol.info + "\n\n"
    }

    if(fx.prologue != undefined) {
        t += fx.prologue + "\n\n"
    }

    if(mem) {
        t += "This function allows to avoid one dynamic memory allocation by storing the object in user-supplied memory. Unless you are hyper-optimizing use **" + fx.name + "** instead.\n\n"
    }

    for(var j = 0; j < fx.args.length; j++) {
        arg = fx.args[j]
        t += "**" + arg.name + "**: " + arg.info + "\n\n"
    }

    if(mem) {
        t += "**mem**: The memory to store the newly created object. It must be at least **" + fx.mem + "** bytes long and must not be deallocated before the object is closed.\n\n"
    }

    if(fx.has_deadline) {
        t += "**deadline**: A point in time when the operation should time out, in milliseconds. Use the now() function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.\n\n"
    }

    t += "\n"

    if(fx.epilogue != undefined) {
        t += fx.epilogue + "\n\n"
    }

    if(fx.protocol != undefined) {
        t += "This function is not available if libdill is compiled with **--disable-sockets** option.\n\n"
        if(fx.protocol.name == "TLS") {
            t += "This function is not available if libdill is compiled without **--enable-tls** option.\n\n"
        }
    }

    /**************************************************************************/
    /*  RETURN VALUE                                                          */
    /**************************************************************************/
    t += "# RETURN VALUE\n\n"
    t += "In case of success the function returns " + fx.result.success + ". "
    t += "In case of error it returns " + fx.result.error + " and sets **errno** to one of the values below.\n\n"

    /**************************************************************************/
    /*  ERRORS                                                                */
    /**************************************************************************/
    t += "# ERRORS\n\n"
    var errs = {}
    if(fx.has_handle_argument) {
        errs['EBADF'] = "Invalid socket handle."
    }
    if(fx.has_deadline) {
        errs['ETIMEDOUT'] = "Deadline was reached."
    }
    if(fx.allocates_memory) {
        errs['ENOMEM'] = "Not enough memory."
    }
    if(fx.allocates_memory) {
        errs['EMFILE'] = "The maximum number of file descriptors in the process are already open."
        errs['ENFILE'] = "The maximum number of file descriptors in the system are already open."
    }
    if(fx.sendsrecvs) {
        errs['ECANCELED'] = "Current coroutine is in the process of shutting down."
        errs['ECONNRESET'] = "Broken connection."
    }
    if(fx.errors != undefined) {
        Object.assign(errs, fx.errors)
    }
    var codes = Object.keys(errs).sort()
    for(var j = 0; j < codes.length; j++) {
        code = codes[j]
        t += "* **" + code + "**: " + errs[code] + "\n"
    }
    t += "\n"

    /**************************************************************************/
    /*  EXAMPLE                                                               */
    /**************************************************************************/
    t += "# EXAMPLE\n\n"
    var example = fx.example
    if(example == undefined) example = fx.protocol.example
    t += "```c" + example + "```\n"

    return t
}

for(var i = 0; i < fxs.length; i++) {
    fx = fxs[i];
    t = generate_man_page(fx, false)
    fs.writeFile(fx.name + ".md", t)
    // Mem functions have no definitions of their own. The docs are generated
    // from the related non-mem function.
    if(fx.mem != undefined) {
        t = generate_man_page(fx, true)
        fs.writeFile(fx.name + "_mem.md", t)
    }
}

