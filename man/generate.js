#!/usr/bin/env nodejs

// This script generates markdown files for libdill documentation.
// These are further processed to generate both UNIX and HTML man pages.

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

http_protocol = {
    name: "HTTP",
    info: "HTTP is an application-level protocol described in RFC 7230. This implementation handles only the request/response exchange. Whatever comes after that must be handled by a different protocol.",
    example: `
int s = tcp_connect(&addr, -1);
s = http_attach(s);
http_sendrequest(s, "GET", "/", -1);
http_sendfield(s, "Host", "www.example.org", -1);
hdone(s, -1);
char reason[256];
http_recvstatus(s, reason, sizeof(reason), -1);
while(1) {
    char name[256];
    char value[256];
    int rc = http_recvfield(s, name, sizeof(name), value, sizeof(value), -1);
    if(rc == -1 && errno == EPIPE) break;
}
s = http_detach(s, -1);
tcp_close(s);
`,
    experimental: true,
}

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

udp_protocol = {
    name: "UDP",
    info: "UDP is an unreliable message-based protocol. The size of the message is limited. The protocol has no initial or terminal handshake. A single socket can be used to different destinations.",
    example: `
struct ipaddr local;
ipaddr_local(&local, NULL, 5555, 0);
struct ipaddr remote;
ipaddr_remote(&remote, "server.example.org", 5555, 0, -1);
int s = udp_open(&local, &remote);
udp_send(s1, NULL, "ABC", 3);
char buf[2000];
ssize_t sz = udp_recv(s, NULL, buf, sizeof(buf), -1);
hclose(s);
`,
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
        name: "http_attach",
        info: "creates HTTP protocol on top of underlying socket",
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
        protocol: http_protocol,
        prologue: "This function instantiates HTTP protocol on top of the underlying protocol.",
        epilogue: "The socket can be cleanly shut down using **http_detach** function.",

        has_handle_argument: true,
        has_deadline: false,
        allocates_memory: true,
        allocates_handle: true,
        sendsrecvs: false,
        mem: "HTTP_SIZE",

        errors: {
            EPROTO: "Underlying socket is not a bytestream socket.",
        },
    },
    {
        name: "http_detach",
        info: "terminates HTTP protocol and returns the underlying socket",
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
        protocol: http_protocol,
        prologue: "This function does the terminal handshake and returns underlying socket to the user. The socket is closed even in the case of error.",

        has_handle_argument: true,
        has_deadline: true,
        allocates_memory: false,
        allocates_handle: false,
        sendsrecvs: true,

        errors: {
            ENOTSUP: "The handle is not an HTTP protocol handle.",
        },
    },
    {
        name: "http_recvfield",
        info: "receives HTTP field from the peer",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "HTTP socket handle.",
           },
           {
               name: "name",
               type: "char*",
               info: "Buffer to store name of the field.",
           },
           {
               name: "namelen",
               type: "size_t",
               info: "Size of the **name** buffer.",
           },
           {
               name: "value",
               type: "char*",
               info: "Buffer to store value of the field.",
           },
           {
               name: "valuelen",
               type: "size_t",
               info: "Size of the **value** buffer.",
           },
        ],
        protocol: http_protocol,
        prologue: "This function receives an HTTP field from the peer.",
        has_handle_argument: true,
        has_deadline: true,
        has_einval: true,
        has_emsgsize: true,
        allocates_memory: false,
        allocates_handle: false,
        sendsrecvs: true,

        errors: {
            EPIPE: "There are no more fields to receive."
        }
    },
    {
        name: "http_recvrequest",
        info: "receives HTTP request from the peer",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "HTTP socket handle.",
           },
           {
               name: "command",
               type: "char*",
               info: "Buffer to store HTTP command in.",
           },
           {
               name: "commandlen",
               type: "size_t",
               info: "Size of the **command** buffer.",
           },
           {
               name: "resource",
               type: "char*",
               info: "Buffer to store HTTP resource in.",
           },
           {
               name: "resourcelen",
               type: "size_t",
               info: "Size of the **resource** buffer.",
           },
        ],
        protocol: http_protocol,
        prologue: "This function receives an HTTP request from the peer.",
        has_handle_argument: true,
        has_deadline: true,
        has_einval: true,
        has_emsgsize: true,
        allocates_memory: false,
        allocates_handle: false,
        sendsrecvs: true,

        example: http_server_example,
    },
    {
        name: "http_recvstatus",
        info: "receives HTTP status from the peer",
        result: {
            type: "int",
            success: "HTTP status code, such as 200 or 404",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "HTTP socket handle.",
           },
           {
               name: "reason",
               type: "char*",
               info: "Buffer to store reason string in.",
           },
           {
               name: "reasonlen",
               type: "size_t",
               info: "Size of the **reason** buffer.",
           },
        ],
        protocol: http_protocol,
        prologue: "This function receives an HTTP status line from the peer.",
        has_handle_argument: true,
        has_deadline: true,
        has_einval: true,
        has_emsgsize: true,
        allocates_memory: false,
        allocates_handle: false,
        sendsrecvs: true,
    },
    {
        name: "http_sendfield",
        info: "sends HTTP field to the peer",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "HTTP socket handle.",
           },
           {
               name: "name",
               type: "const char*",
               info: "Name of the field.",
           },
           {
               name: "value",
               type: "const char*",
               info: "Value of the field.",
           },
        ],
        protocol: http_protocol,
        prologue: "This function sends an HTTP field, i.e. a name/value pair, to the peer. For example, if name is **Host** and resource is **www.example.org** the line sent will look like this:\n\n```\nHost: www.example.org\n```\n\nAfter sending the last field of HTTP request don't forget to call **hdone** on the socket. It will send an empty line to the server to let it know that the request is finished and it should start processing it.",
        has_handle_argument: true,
        has_deadline: true,
        has_einval: true,
        allocates_memory: false,
        allocates_handle: false,
        sendsrecvs: true,
    },
    {
        name: "http_sendrequest",
        info: "sends initial HTTP request",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "HTTP socket handle.",
           },
           {
               name: "command",
               type: "const char*",
               info: "HTTP command, such as **GET**.",
           },
           {
               name: "resource",
               type: "const char*",
               info: "HTTP resource, such as **/index.html**.",
           },
        ],
        protocol: http_protocol,
        prologue: "This function sends an initial HTTP request with the specified command and resource.  For example, if command is **GET** and resource is **/index.html** the line sent will look like this:\n\n```\nGET /index.html HTTP/1.1\n```",
        has_handle_argument: true,
        has_deadline: true,
        has_einval: true,
        allocates_memory: false,
        allocates_handle: false,
        sendsrecvs: true,
    },
    {
        name: "http_sendstatus",
        info: "sends HTTP status to the peer",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "HTTP socket handle.",
           },
           {
               name: "status",
               type: "int",
               info: "HTTP status such as 200 or 404.",
           },
           {
               name: "reason",
               type: "const char*",
               info: "Reason string such as 'OK' or 'Not found'.",
           },
        ],
        protocol: http_protocol,
        prologue: "This function sends an HTTP status line to the peer. It is meant to be done at the beginning of the HTTP response. For example, if status is 404 and reason is 'Not found' the line sent will look like this:\n\n```\nHTTP/1.1 404 Not found\n```",
        has_handle_argument: true,
        has_deadline: true,
        has_einval: true,
        allocates_memory: false,
        allocates_handle: false,
        sendsrecvs: true,

        example: http_server_example,
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
        has_einval: true,
        allocates_memory: true,
        allocates_handle: true,
        sendsrecvs: true,
        mem: "TLS_SIZE",

        errors: {
            EPROTO: "Underlying socket is not a bytestream socket.",
        },

        example: `
int s = tcp_accept(listener, NULL, -1);
s = tls_attach_server(s, -1);
bsend(s, "ABC", 3, -1);
char buf[3];
ssize_t sz = brecv(s, buf, sizeof(buf), -1);
s = tls_detach(s, -1);
tcp_close(s);
`,
    },
    {
        name: "tls_detach",
        info: "terminates TLS protocol and returns the underlying socket",
        result: {
            type: "int",
            success: "underlying socket handle",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the TLS socket.",
           },
        ],
        protocol: tls_protocol,
        prologue: "This function does the terminal handshake and returns underlying socket to the user. The socket is closed even in the case of error.",

        has_handle_argument: true,
        has_deadline: true,
        allocates_memory: false,
        allocates_handle: false,
        sendsrecvs: true,

        errors: {
            ENOTSUP: "The handle is not a TLS protocol handle.",
        },
    },
    {
        name: "udp_open",
        info: "opens an UDP socket",
        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
           {
               name: "local",
               type: "struct ipaddr*",
               info: "IP  address to be used to set source IP address in outgoing packets. Also, the socket will receive packets sent to this address. If port in the address is set to zero an ephemeral port will be chosen and filled into the local address.",
           },
           {
               name: "remote",
               type: "struct ipaddr*",
               info: "IP address used as default destination for outbound packets. It is used when destination address in **udp_send** function is set to **NULL**. It is also used by **msend** and **mrecv** functions which don't allow to specify the destination address explicitly.",
           },
        ],
        protocol: udp_protocol,
        prologue: "This function creates an UDP socket.",
        epilogue: "To close this socket use **hclose** function.",

        has_handle_argument: false,
        has_deadline: false,
        has_einval: true,
        allocates_memory: true,
        allocates_handle: true,
        sendsrecvs: false,
        mem: "UDP_SIZE",

        errors: {
            EADDRINUSE: "The local address is already in use.",
            EADDRNOTAVAIL: "The specified address is not available from the local machine.",
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
        t += "**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.\n\n"
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
    if(fx.has_einval) {
        errs['EINVAL'] = "Invalid argument."
    }
    if(fx.has_emsgsize) {
        errs['EMSGSIZE'] = "The data won't fit into the supplied buffer."
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

