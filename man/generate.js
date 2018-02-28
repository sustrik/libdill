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
            postfix: part of the type to go after the name (e.g. "[]")
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
              // size if the size macro such as TCP_SIZE or UDP_SIZE
*/

var fs = require('fs');

standard_errors = {
    EBADF: "Invalid handle.",
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
    msleep(now() + 1000);
    /* Cancel any workers that are still running. */
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

    \`\`\`
    go(bar(foo(a)));
    \`\`\`

    Do this:

    \`\`\`
    int a = foo();
    go(bar(a));
    \`\`\`
`

crlf_protocol = {
    section: "CRLF protocol",
    type: "message",
    info: `
        CRLF is a message-based protocol that delimits messages usign CR+LF byte
        sequence (0x0D 0x0A). In other words, it's a protocol to send text
        messages separated by newlines. The protocol has no initial handshake.
        Terminal handshake is accomplished by each peer sending an empty line.
    `,
    example: `
        int s = tcp_connect(&addr, -1);
        s = crlf_attach(s);
        msend(s, "ABC", 3, -1);
        char buf[256];
        ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
        s = crlf_detach(s, -1);
        tcp_close(s);
    `,
}

http_protocol = {
    section: "HTTP protocol",
    type: "application",
    info: `
        HTTP is an application-level protocol described in RFC 7230. This
        implementation handles only the request/response exchange. Whatever
        comes after that must be handled by a different protocol.
    `,
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

ipc_protocol = {
    section: "IPC protocol",
    type: "bytestream",
    info: `
       IPC  protocol is a bytestream protocol for transporting data among
       processes on the same machine.  It is an equivalent to POSIX
       **AF_LOCAL** sockets.
    `,
    example: `
        int ls = ipc_listen("/tmp/test.ipc", 10);
        int s = ipc_accept(ls, -1);
        bsend(s, "ABC", 3, -1);
        char buf[3];
        brecv(s, buf, sizeof(buf), -1);
        ipc_close(s);
        ipc_close(ls);
    `
}

pfx_protocol = {
    section: "PFX protocol",
    type: "message",
    info: `
        PFX  is a message-based protocol to send binary messages prefixed by
        8-byte size in network byte order. The protocol has no initial
        handshake. Terminal handshake is accomplished by each peer sending eight
        0xFF bytes.
    `,
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

tcp_protocol = {
    section: "TCP protocol",
    type: "bytestream",
    info: `
       TCP protocol is a reliable bytestream protocol for transporting data
       over network. It is defined in RFC 793.
    `,
    example: `
        struct ipaddr addr;
        ipaddr_local(&addr, NULL, 5555, 0);
        int ls = tcp_listen(&addr, 10);
        int s = tcp_accept(ls, NULL, -1);
        bsend(s, "ABC", 3, -1);
        char buf[3];
        brecv(s, buf, sizeof(buf), -1);
        tcp_close(s);
        tcp_close(ls);
    `
}

tls_protocol = {
    section: "TLS protocol",
    type: "bytestream",
    info: `
        TLS is a cryptographic protocol to provide secure communication over
        the network. It is a bytestream protocol.
    `,
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
    section: "UDP protocol",
    type: "message",
    info: `
        UDP is an unreliable message-based protocol defined in RFC 768. The size
        of the message is limited. The protocol has no initial or terminal
        handshake. A single socket can be used to different destinations.
    `,
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
        name: "brecv",
        section: "Message sockets",
        info: "receives data from a bytestream socket",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket.",
           },
           {
               name: "buf",
               type: "void*",
               info: "Buffer to receive the data to.",
           },
           {
               name: "len",
               type: "size_t",
               info: "Size of the buffer, in bytes.",
           },
        ],
        prologue: `
            This function receives data from a bytestream socket. It is a
            blocking operation that unblocks only after the requested amount of
            data is received.  There is no such thing as partial receive.
            If a problem, including timeout, occurs while receiving the data,
            an error is returned to the user and the socket cannot be used for
            receiving from that point on.

            If **buf** is **NULL**, **len** bytes will be received but they will
            be dropped rather than returned to the user.
        `,

        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        errors: ["EINVAL"],
        custom_errors: {
            EPIPE: "Closed connection.",
        },

        example: `
            char msg[100];
            int rc = brecv(s, msg, sizeof(msg), -1);
        `,
    },
    {
        name: "brecvl",
        section: "Message sockets",
        info: "receives data from a bytestream socket",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket.",
           },
        ],

        prologue: `
            This function receives data from a bytestream socket. It is a
            blocking operation that unblocks only after the requested amount of
            data is received.  There is no such thing as partial receive.
            If a problem, including timeout, occurs while receiving the data,
            an error is returned to the user and the socket cannot be used for
            receiving from that point on.
        `,

        has_handle_argument: true,
        has_deadline: true,
        has_iol: true,
        uses_connection: true,

        errors: ["EINVAL"],
        custom_errors: {
            EPIPE: "Closed connection.",
        },
    },
    {
        name: "bsend",
        section: "Bytestream sockets",
        info: "sends data to a socket",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket to send the data to.",
           },
           {
               name: "buf",
               type: "const void*",
               info: "Buffer to send.",
           },
           {
               name: "len",
               type: "size_t",
               info: "Size of the buffer, in bytes.",
           },
        ],

        prologue: `
            This function sends data to a bytestream socket. It is a blocking
            operation that unblocks only after all the data are sent. There is
            no such thing as partial send. If a problem, including timeout,
            occurs while sending the data error is returned to the user and the
            socket cannot be used for sending from that point on.
        `,

        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        errors: ["EINVAL"],
        custom_errors: {
            EPIPE: "Closed connection.",
        },

        example: `
            int rc = bsend(s, "ABC", 3, -1);
        `,
    },
    {
        name: "bsendl",
        section: "Bytestream sockets",
        info: "sends data to a socket",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket to send the data to.",
           },
        ],

        prologue: `
            This function sends data to a bytestream socket. It is a blocking
            operation that unblocks only after all the data are sent. There is
            no such thing as partial send. If a problem, including timeout,
            occurs while sending the data error is returned to the user and the
            socket cannot be used for sending from that point on.
        `,

        has_handle_argument: true,
        has_deadline: true,
        has_iol: true,
        uses_connection: true,

        errors: ["EINVAL"],
        custom_errors: {
            EPIPE: "Closed connection.",
        },
    },
    {
        name: "bundle",
        section: "Coroutines",
        info: "create an empty coroutine bundle",
        result: {
            type: "int",
            success: "handle of the newly create coroutine bundle",
            error: "-1",
        },

        prologue: `
            Coroutines are always running in bundles. Even a single coroutine
            created by **go** gets its own bundle. A bundle is a lifetime
            control mechanism. When it is closed, all coroutines within the
            bundle are canceled.

            This function creates an empty bundle. Coroutines can be added to
            the bundle using the **bundle_go** and **bundle_go_mem** functions.

            If **hdone** is called on a bundle, it waits until all coroutines
            exit. After calling **hdone**, irrespective of whether it succeeds
            or times out, no further coroutines can be launched using the
            bundle.

            When **hclose()** is called on the bundle, all the coroutines
            contained in the bundle will be canceled. In other words, all the
            blocking functions within the coroutine will start failing with an
            **ECANCELED** error. The **hclose()** function itself won't exit
            until all the coroutines in the bundle exit.
        `,

        allocates_handle: true,

        example: bundle_example,

        mem: "BUNDLE_SIZE",
    },
    {
        name: "bundle_go",
        section: "Coroutines",
        info: "launches a coroutine within a bundle",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "bndl",
                type: "int",
                info: "Bundle to launch the coroutine in.",
            },
            {
                name: "expression",
                info: "Expression to evaluate as a coroutine.",
            },
        ],

        prologue: `
            This construct launches a coroutine within the specified bundle.
            For more information about bundles see **bundle**.

            The coroutine gets a 1MB stack.
            The stack is guarded by a non-writeable memory page. Therefore,
            stack overflow will result in a **SEGFAULT** rather than overwriting
            memory that doesn't belong to it.
        `,
        epilogue: go_info,

        has_handle_argument: true,

        errors: ["ECANCELED", "ENOMEM"],

        example: bundle_example,
    },
    {
        name: "bundle_go_mem",
        section: "Coroutines",
        info: "launches a coroutine within a bundle",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "bndl",
                type: "int",
                info: "Bundle to launch the coroutine in.",
            },
            {
                name: "expression",
                info: "Expression to evaluate as a coroutine.",
            },
            {
                name: "mem",
                type: "void*",
                info: "Buffer to store coroutine's stack.",
            },
            {
                name: "memlen",
                type: "size_t",
                info: "Size of th buffer, in bytes.",
            },
        ],

        prologue: `
            This construct launches a coroutine within the specified bundle.
            For more information about bundles see **bundle**.

            The stack for the coroutine is supplied by the user.
            The stack has no guard page and stack overflow will result in
            overwriting memory.
        `,
        epilogue: go_info,

        has_handle_argument: true,

        errors: ["ECANCELED"],

        example: bundle_example,
    },
    {
        name: "chmake",
        section: "Channels",
        info: "creates a channel",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "chv",
                type: "int",
                postfix: "[2]",
                info: "Out parameter. Two handles to the opposite ends of the channel."
            },
        ],

        prologue: `
            Creates a bidirectional channel. In case of success handles to the
            both sides of the channel will be returned in **chv** parameter.

            A channel is a synchronization primitive, not a container.
            It doesn't store any items.
        `,

        errors: ['ECANCELED', 'ENOMEM'],

        example: `
            int ch[2];
            int rc = chmake(ch);
            if(rc == -1) {
                perror("Cannot create channel");
                exit(1);
            }
        `,

        mem: "CHSIZE",
    },
    {
        name: "choose",
        section: "Channels",
        info: "performs one of multiple channel operations",

        add_to_synopsis: `
            struct chclause {
                int op;
                int ch;
                void *val;
                size_t len;
            };
        `,

        result: {
            type: "int",
            success: "index of the clause that caused the function to exit",
            error: "-1",
            info: `
                Even if an index is returned, **errno** may still be set to
                an error value. The operation was successfull only if **errno**
                is set to zero.
            `,
        },

        args: [
            {
                name: "clauses",
                type: "struct chclause*",
                info: "Operations to choose from. See below."
            },
            {
                name: "nclauses",
                type: "int",
                info: "Number of clauses."
            },
        ],

        has_deadline: true,

        prologue: `
            Accepts a list of channel operations. Performs one that can be done
            first. If multiple operations can be done immediately, the one that
            comes earlier in the array is executed.
        `,

        epilogue: `
            The fields in **chclause** structure are as follows:

            * **op**: Operation to perform. Either **CHSEND** or **CHRECV**.
            * **ch**: The channel to perform the operation on.
            * **val**: Buffer containing the value to send or receive.
            * **len**: Size of the buffer.
        `,

        errors: ['EINVAL'],

        add_to_errors: `
            Additionally, if the function returns an index it can set **errno**
            to one of the following values:

            * **0**: Operation was completed successfully.
            * **EBADF**: Invalid handle.
            * **EINVAL**: Invalid parameter.
            * **EMSGSIZE**: The peer expected a message with different size.
            * **ENOTSUP**: Operation not supported. Presumably, the handle isn't a channel.
            * **EPIPE**: Channel has been closed with **hdone**.
        `,

        example: `
            int val1 = 0;
            int val2;
            struct chclause clauses[] = {
                {CHSEND, ch, &val1, sizeof(val1)},
                {CHRECV, ch, &val2, sizeof(val2)}
            };
            int rc = choose(clauses, 2, now() + 1000);
            if(rc == -1) {
                perror("Choose failed");
                exit(1);
            }
            if(rc == 0) {
                printf("Value %d sent.\\n", val1);
            }
            if(rc == 1) {
                printf("Value %d received.\\n", val2);
            }
        `,
    },
    {
        name: "chrecv",
        section: "Channels",
        info: "receives a message from a channel",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "ch",
                type: "int",
                info: "The channel."
            },
            {
                name: "val",
                type: "void*",
                info: "The buffer to receive the message to."
            },
            {
                name: "len",
                type: "size_t",
                info: "Size of the buffer, in bytes."
            },
        ],

        has_deadline: true,

        prologue: `
            Receives a message from a channel.

            The size of the message requested from the channel must match the
            size of the message sent to the channel. Otherwise, both peers fail
            with **EMSGSIZE** error. 

            If there's no one sending a message at the moment, the function
            blocks until someone shows up or until the deadline expires.
        `,

        has_handle_argument: true,

        errors: ['EINVAL'],

        custom_errors: {
            EMSGSIZE: "The peer sent a message of different size.",
            EPIPE: "Channel has been closed with hdone.",
        },

        example: `
            int val;
            int rc = chrecv(ch, &val, sizeof(val), now() + 1000);
            if(rc != 0) {
                perror("Cannot receive message");
                exit(1);
            }
            printf("Value %d received.\\n", val);
        `,
    },
    {
        name: "chsend",
        section: "Channels",
        info: "sends a message to a channel",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "ch",
                type: "int",
                info: "The channel."
            },
            {
                name: "val",
                type: "const void*",
                info: "Pointer to the value to send to the channel."
            },
            {
                name: "len",
                type: "size_t",
                info: "Size of the value to send to the channel, in bytes."
            },
        ],

        has_deadline: true,

        prologue: `
            Sends a message to a channel.

            The size of the message sent to the channel must match the size of
            the message requested from the channel. Otherwise, both peers fail
            with **EMSGSIZE** error. 

            If there's no receiver for the message, the function blocks until
            one shows up or until the deadline expires.
        `,

        has_handle_argument: true,

        errors: ['EINVAL'],

        custom_errors: {
            EMSGSIZE: "The peer expected a different message size.",
            EPIPE: "Channel has been closed with hdone.",
        },

        example: `
            int val = 42;
            int rc = chsend(ch, &val, sizeof(val), now() + 1000);
            if(rc != 0) {
                perror("Cannot send message");
                exit(1);
            }
            printf("Value %d sent successfully.\\n", val);
        `,
    },
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
               info: "Handle of the underlying socket. It must be a " +
                     "bytestream protocol.",
           },
        ],
        protocol: crlf_protocol,
        prologue: `
            This function instantiates CRLF protocol on top of the underlying
            protocol.
        `,
        epilogue: `
            The socket can be cleanly shut down using **crlf_detach** function.
        `,
        has_handle_argument: true,
        allocates_handle: true,
        mem: "CRLF_SIZE",
        custom_errors: {
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
        prologue: `
            This function does the terminal handshake and returns underlying
            socket to the user. The socket is closed even in the case of error.
        `,
        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        custom_errors: {
            ENOTSUP: "The handle is not a CRLF protocol handle.",
        },
    },
    {
        name: "fdclean",
        section: "File descriptors",
        info: "erases cached info about a file descriptor",
      
        args: [
            {
                name: "fd",
                type: "int",
                info: "file descriptor (OS-level one, not a libdill handle)"
            },
        ],

        prologue: `
            This function drops any state that libdill associates with a file
            descriptor. It has to be called before the file descriptor is
            closed. If it is not, the behavior is undefined.

            It should also be used whenever you are losing control of the file
            descriptor. For example, when passing it to a third-party library.
            Also, if you are handed the file descriptor by third party code
            you should call this function just before returning it back to the
            original owner.
        `,

        example: `
            int fds[2];
            pipe(fds);
            use_the_pipe(fds);
            fdclean(fds[0]);
            close(fds[0]);
            fdclean(fds[1]);
            close(fds[1]);
        `,
    },
    {
        name: "fdin",
        section: "File descriptors",
        info: "waits on a file descriptor to become readable",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
      
        args: [
            {
                name: "fd",
                type: "int",
                info: "file descriptor (OS-level one, not a libdill handle)"
            },
        ],

        prologue: `
            Waits on a file descriptor (true OS file descriptor, not libdill
            socket handle) to either become readable or get into an error state.
            Either case leads to a successful return from the function. To
            distinguish the two outcomes, follow up with a read operation on
            the file descriptor.
        `,

        has_deadline: true,

        custom_errors: {
            "EBADF": "Not a file descriptor.",
            "EBUSY": "Another coroutine already blocked on **fdin** with this file descriptor.",
        },

        example: `
            int result = fcntl(fd, F_SETFL, O_NONBLOCK);
            assert(result == 0);
            while(1) {
                result = fdin(fd, -1);
                assert(result == 0);
                char buf[1024];
                ssize_t len = recv(fd, buf, sizeof(buf), 0);
                assert(len > 0);
                process_input(buf, len);
            }
        `,
    },
    {
        name: "fdout",
        section: "File descriptors",
        info: "wait on file descriptor to become writable",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
      
        args: [
            {
                name: "fd",
                type: "int",
                info: "file descriptor (OS-level one, not a libdill handle)"
            },
        ],

        prologue: `
            Waits on a file descriptor (true OS file descriptor, not libdill
            socket handle) to either become writeable or get into an error
            state. Either case leads to a successful return from the function.
            To distinguish the two outcomes, follow up with a write operation on
            the file descriptor.
        `,

        has_deadline: true,

        custom_errors: {
            "EBADF": "Not a file descriptor.",
            "EBUSY": "Another coroutine already blocked on **fdout** with this file descriptor.",
        },

        example: `
            int result = fcntl(fd, F_SETFL, O_NONBLOCK);
            assert(result == 0);
            while(len) {
                result = fdout(fd, -1);
                assert(result == 0);
                ssize_t sent = send(fd, buf, len, 0);
                assert(len > 0);
                buf += sent;
                len -= sent;
            }
        `,
    },
    {
        name: "go",
        section: "Coroutines",
        info: "launches a coroutine",

        result: {
            type: "int",
            success: "handle of a bundle containing the new coroutine",
            error: "-1",
            info: "For details on coroutine bundles see **bundle** function.",
        },

        args: [
            {
                name: "expression",
                info: "Expression to evaluate as a coroutine.",
            }
        ],

        allocates_handle: true,

        prologue: `
            This construct launches a coroutine. The coroutine gets a 1MB stack.
            The stack is guarded by a non-writeable memory page. Therefore,
            stack overflow will result in a **SEGFAULT** rather than overwriting
            memory that doesn't belong to it.
        `,
        epilogue: go_info,

        errors: ["ECANCELED", "ENOMEM"],

        example: `
            coroutine void add(int a, int b) {
                printf("%d+%d=%d\\n", a, b, a + b);
            }

            ...

            int h = go(add(1, 2));
        `,
    },
    {
        name: "go_mem",
        section: "Coroutines",
        info: "launches a coroutine",

        result: {
            type: "int",
            success: "handle of a bundle containing the new coroutine",
            error: "-1",
            info: "For details on coroutine bundles see **bundle** function.",
        },

        args: [
            {
                name: "expression",
                info: "Expression to evaluate as a coroutine.",
            },
            {
                name: "mem",
                type: "void*",
                info: "Buffer to hold the coroutine's stack.",
            },
            {
                name: "memlen",
                type: "size_t",
                info: "Size of the **mem** buffer.",
            },
        ],

        allocates_handle: true,

        prologue: `
            This construct launches a coroutine on a user-supplied stack.
            The stack has no guard page and stack overflow will result in
            overwriting memory.
        `,
        epilogue: go_info,

        errors: ["ECANCELED"],

        example: `
              coroutine void add(int a, int b) {
                  printf("%d+%d=%d\\n", a, b, a + b);
              }

              ...
              char mem[16384];
              int h = go_mem(add(1, 2), mem, sizeof(mem));
        `,
    },
    {
        name: "hclose",
        section: "Handles",
        info: "hard-closes a handle",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "h",
                type: "int",
                info: "The handle.",
            },
        ],
    
        has_handle_argument: true,

        prologue: `
            This function closes a handle. Once all handles pointing to the same
            underlying object have been closed, the object is deallocated
            immediately, without blocking.

            The  function  guarantees that all associated resources are
            deallocated. However, it does not guarantee that the handle's work
            will have been fully finished. E.g., outbound network data may not
            be flushed.

            In the case of network protocol sockets the entire protocol stack
            is closed, the topmost protocol as well as all the protocols
            beneath it.
        `,

        example: `
            int ch[2];
            chmake(ch);
            hclose(ch[0]);
            hclose(ch[1]);
        `
    },
    {
        name: "hdone",
        section: "Handles",
        info: "announce end of input to a handle",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "h",
                type: "int",
                info: "The handle.",
            },
        ],

        has_handle_argument: true,
        has_deadline: true,

        prologue: `
            This function is used to inform the handle that there will be no
            more input. This gives it time to finish it's work and possibly
            inform the user when it is safe to close the handle.

            For example, in case of TCP protocol handle, hdone would send out
            a FIN packet. However, it does not wait until it is acknowledged
            by the peer.
 
            After hdone is called on a handle, any attempts to send more data
            to the handle will result in EPIPE error.

           Handle implementation may also decide to prevent any further
           receiving of data and return EPIPE error instead.
        `,

        custom_errors: {
            "EPIPE": "Pipe broken. **hdone** was already called either on this or the other side of the connection."
        },

        example: `
            http_sendrequest(s, "GET", "/" -1);
            http_sendfield(s, "Host", "www.example.org", -1);
            hdone(s, -1);
            char reason[256];
            int status = http_recvstatus(s, reason, sizeof(reason), -1);
        `
    },
    {
        name: "hdup",
        section: "Handles",
        info: "duplicates a handle",

        result: {
            type: "int",
            success: "newly duplicated handle",
            error: "-1",
        },
        args: [
            {
                name: "h",
                type: "int",
                info: "Handle to duplicate.",
            }
        ],

        allocates_handle: true,

        prologue: `
            Duplicates a handle. The new handle will refer to the same
            underlying object.
        `,
        epilogue: `
            Each duplicate of a handle requires its own call to **hclose**.
            The underlying object is deallocated when all handles pointing to it
            have been closed.
        `,

        errors: ["EBADF"],

        example: `
            int h1 = tcp_connect(&addr, deadline);
            h2 = hdup(h1);
            hclose(h1);
            hclose(h2); /* The socket gets deallocated here. */
        `
    },
    {
        name: "hmake",
        section: "Handles",
        info: "creates a handle",
        is_in_libdillimpl: true,
        add_to_synopsis: `
            struct hvfs {
                void *(*query)(struct hvfs *vfs, const void *type);
                void (*close)(int h);
                int (*done)(struct hvfs *vfs, int64_t deadline);
            };
        `,
        result: {
            type: "int",
            success: "newly created handle",
            error: "-1",
        },
        args: [
            {
                name: "hvfs",
                type: "struct hvfs*",
                info: "virtual-function table of operations associated with " +
                      "the handle",
            }
        ],

        prologue: `
            A handle is the user-space equivalent of a file descriptor.
            Coroutines and channels are represented by handles.

            Unlike with file descriptors, however, you can use the **hmake**
            function to create your own type of handle.

            The argument of the function is a virtual-function table of
            operations associated with the handle.

            When implementing the **close** operation, keep in mind that
            invoking blocking operations is not allowed, as blocking operations
            invoked within the context of a **close** operation will fail with
            an **ECANCELED** error.

            To close a handle, use the **hclose** function.
        `,

        errors: ["ECANCELED", "EINVAL", "ENOMEM"],
    },
    {
        name: "hquery",
        section: "Handles",
        info: "gets an opaque pointer associated with a handle and a type",
        is_in_libdillimpl: true,

        result: {
            type: "void*",
            success: "opaque pointer",
            error: "NULL",
        },
        args: [
            {
                name: "h",
                type: "int",
                info: "The handle.",
            }
        ],

        prologue: `
            Returns an opaque pointer associated with the passed handle and
            type.  This function is a fundamental construct for building APIs
            on top of handles.

            The type argument is not interpreted in any way. It is used only
            as an unique ID.  A unique ID can be created, for instance, like
            this:

            \`\`\`
            int foobar_placeholder = 0;
            const void *foobar_type = &foobar_placeholder;
            \`\`\`

            The  return value has no specified semantics. It is an opaque
            pointer.  One typical use case for it is to return a pointer to a
            table of function pointers.  These function pointers can then be
            used to access the handle's functionality (see the example).

            Pointers returned by **hquery** are meant to be cachable. In other
            words, if you call hquery on the same handle with the same type
            multiple times, the result should be the same.
        `,

        errors: ["EBADF"],

        example: `
            struct quux_vfs {
                int (*foo)(struct quux_vfs *vfs, int a, int b);
                void (*bar)(struct quux_vfs *vfs, char *c);
            };

            int quux_foo(int h, int a, int b) {
                struct foobar_vfs *vfs = hquery(h, foobar_type);
                if(!vfs) return -1;
                return vfs->foo(vfs, a, b);
            }

            void quux_bar(int h, char *c) {
                struct foobar_vfs *vfs = hquery(h, foobar_type);
                if(!vfs) return -1;
                vfs->bar(vfs, c);
            }
        `
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
               info: "Handle of the underlying socket. It must be a " +
                     "bytestream protocol.",
           },
        ],
        protocol: http_protocol,
        prologue: `
            This function instantiates HTTP protocol on top of the underlying
            protocol.
        `,
        epilogue: `
            The socket can be cleanly shut down using **http_detach** function.
        `,
        has_handle_argument: true,
        allocates_handle: true,
        mem: "HTTP_SIZE",

        custom_errors: {
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
        prologue: `
            This function does the terminal handshake and returns underlying
            socket to the user. The socket is closed even in the case of error.
        `,

        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        custom_errors: {
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
        uses_connection: true,

        errors: ["EINVAL", "EMSGSIZE"],
        custom_errors: {
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
        uses_connection: true,

        errors: ["EINVAL", "EMSGSIZE"],

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
        uses_connection: true,

        errors: ["EINVAL", "EMSGSIZE"],
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
        prologue: `
            This function sends an HTTP field, i.e. a name/value pair, to the
            peer. For example, if name is **Host** and resource is
            **www.example.org** the line sent will look like this:

            \`\`\`
            Host: www.example.org
            \`\`\`

            After sending the last field of HTTP request don't forget to call
            **hdone** on the socket. It will send an empty line to the server to
            let it know that the request is finished and it should start
            processing it.
        `,
        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        errors: ["EINVAL"],
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
        prologue: `
            This function sends an initial HTTP request with the specified
            command and resource.  For example, if command is **GET** and
            resource is **/index.html** the line sent will look like this:

            \`\`\`
            GET /index.html HTTP/1.1
            \`\`\`
        `,
        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        errors: ["EINVAL"],
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
        prologue: `
            This function sends an HTTP status line to the peer. It is meant to
            be done at the beginning of the HTTP response. For example, if
            status is 404 and reason is 'Not found' the line sent will look like
            this:

            \`\`\`
            HTTP/1.1 404 Not found
            \`\`\`
        `,
        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        errors: ["EINVAL"],

        example: http_server_example,
    },
    {
        name: "ipaddr_family",
        section: "IP addresses",
        info: "returns family of the IP address",
        result: {
            type: "int",
            info: "IP family.",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
        ],

        prologue: `
            Returns family of the address, i.e.  either AF_INET for IPv4
            addresses or AF_INET6 for IPv6 addresses.
        `,

        example: ipaddr_example,
    },
    {
        name: "ipaddr_len",
        section: "IP addresses",
        info: "returns length of the address",
        result: {
            type: "int",
            info: "Length of the IP address.",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
        ],

        prologue: `
            Returns lenght of the address, in bytes. This function is typically
            used in combination with **ipaddr_sockaddr** to pass address and its
            length to POSIX socket APIs.
        `,

        example: ipaddr_example,
    },
    {
        name: "ipaddr_local",
        section: "IP addresses",
        info: "resolve the address of a local network interface",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "struct ipaddr*",
                info: "Out parameter, The IP address object.",
            },
            {
                name: "name",
                type: "const char*",
                info: "Name of the local network interface, such as \"eth0\", \"192.168.0.111\" or \"::1\".",
            },
            {
                name: "port",
                type: "int",
                info: "Port number. Valid values are 1-65535.",
            },
            {
                name: "mode",
                type: "int",
                info: "What kind of address to return. See above.",
            },
        ],

        prologue: trimrect(`
            Converts an IP address in human-readable format, or a name of a
            local network interface into an **ipaddr** structure.
        `) + "\n\n" + trimrect(ipaddr_mode),

        custom_errors: {
            ENODEV: "Local network interface with the specified name does not exist."
        },

        example: `
            struct ipaddr addr;
            ipaddr_local(&addr, "eth0", 5555, 0);
            int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
            bind(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
        `,
    },
    {
        name: "ipaddr_port",
        section: "IP addresses",
        info: "returns the port part of the address",
        result: {
            type: "int",
            info: "The port number.",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
        ],

        prologue: `
            Returns the port part of the address.
        `,

        example: `
            int port = ipaddr_port(&addr);
        `,
    },
    {
        name: "ipaddr_remote",
        section: "IP addresses",
        info: "resolve the address of a remote IP endpoint",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "struct ipaddr*",
                info: "Out parameter, The IP address object.",
            },
            {
                name: "name",
                type: "const char*",
                info: "Name of the remote IP endpoint, such as \"www.example.org\" or \"192.168.0.111\".",
            },
            {
                name: "port",
                type: "int",
                info: "Port number. Valid values are 1-65535.",
            },
            {
                name: "mode",
                type: "int",
                info: "What kind of address to return. See above.",
            },
        ],

        has_deadline: true,

        prologue: trimrect(`
            Converts an IP address in human-readable format, or a name of a
            remote host into an **ipaddr** structure.
        `) + "\n\n" + trimrect(ipaddr_mode),

        custom_errors: {
            "EADDRNOTAVAIL": "The name of the remote host cannot be resolved to an address of the specified type.",
        },

        example: ipaddr_example,
    },
    {
        name: "ipaddr_setport",
        section: "IP addresses",
        info: "changes port number of the address",
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
        ],

        prologue: `
            Changes port number of the address.
        `,

        example: `
            ipaddr_setport(&addr, 80);
        `,
    },
    {
        name: "ipaddr_sockaddr",
        section: "IP addresses",
        info: "returns sockaddr structure corresponding to the IP address",
        result: {
            type: "const struct sockaddr*",
            info: "Pointer to **sockaddr** structure correspoding to the address object.",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
        ],

        prologue: `
            Returns **sockaddr** structure corresponding to the IP address.
            This function is typically used in combination with ipaddr_len to
            pass address and its length to POSIX socket APIs.
        `,

        example: ipaddr_example,
    },
    {
        name: "ipaddr_str",
        section: "IP addresses",
        info: "convert address to a human-readable string",
        result: {
            type: "const char*",
            info: "The function returns **ipstr** argument, i.e.  pointer to the formatted string.",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
            {
                name: "buf",
                type: "char*",
                info: "Buffer to store the result in. It must be at least **IPADDR_MAXSTRLEN** bytes long.",
            },
        ],

        prologue: "Formats address as a human-readable string.",

        example: `
              char buf[IPADDR_MAXSTRLEN];
              ipaddr_str(&addr, buf);
        `,
    },
    {
        name: "ipc_accept",
        info: "accepts an incoming IPC connection",

        result: {
            type: "int",
            success: "handle of the new connection",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "Socket created by **ipc_listen**.",
            },
        ],

        has_deadline: true,

        protocol: ipc_protocol,

        prologue: `
            This function accepts an incoming IPC connection.
        `,
        epilogue: `
            The socket can be cleanly shut down using **ipc_close** function.
        `,

        has_handle_argument: true,
        allocates_handle: true,
        mem: "IPC_SIZE",
    },
    {
        name: "ipc_close",
        info: "closes IPC connection in an orderly manner",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "The IPC socket.",
            },
        ],

        has_deadline: true,

        protocol: ipc_protocol,

        prologue: `
            This function closes a IPC socket cleanly. Unlike **hclose** it lets
            the peer know that it is shutting down and waits till the peer
            acknowledged the shutdown. If this terminal handshake cannot be
            done it returns error. The socket is closed even in the case of
            error.

            It can also be used to close IPC listener socket in which case it's
            equivalent to calling **hclose**.
        `,

        has_handle_argument: true,
        custom_errors: {
            ECONNRESET: "Broken connection.",
        },
    },
    {
        name: "ipc_connect",
        info: "creates a connection to remote IPC endpoint",

        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "const char*",
                info: "Filename to connect to.",
            },
        ],

        has_deadline: true,

        protocol: ipc_protocol,

        prologue: `
            This function creates a connection to a remote IPC endpoint.
        `,
        epilogue: `
            The socket can be cleanly shut down using **ipc_close** function.
        `,

        allocates_handle: true,
        mem: "IPC_SIZE",

        errors: ["EINVAL"],
        custom_errors: {
            EACCES: "The process does not have appropriate privileges.",
            ECONNREFUSED: "The target address was not listening for connections or refused the connection request.",
            ECONNRESET: "Remote host reset the connection request.",
            ELOOP: "A loop exists in symbolic links encountered during resolution of the pathname in address.",
            ENAMETOOLONG: "A component of a pathname exceeded **NAME_MAX** characters, or an entire pathname exceeded **PATH_MAX** characters.",
            ENOENT: "A component of the pathname does not name an existing file or the pathname is an empty string.",
            ENOTDIR: "A component of the path prefix of the pathname in address is not a directory.",
        },

        example: `
            int s = tcp_connect("/tmp/test.ipc", -1);
            bsend(s, "ABC", 3, -1);
            char buf[3];
            brecv(s, buf, sizeof(buf), -1);
            ipc_close(s);
        `
    },
    {
        name: "ipc_listen",
        info: "starts listening for incoming IPC connections",

        result: {
            type: "int",
            success: "newly created socket",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "const char*",
                info: "The filename to listen on.",
            },
            {
                name: "backlog",
                type: "int",
                info: "Maximum number of connections that can be kept open without accepting them.",
            },
        ],

        protocol: ipc_protocol,

        prologue: `
            This function starts listening for incoming IPC connections.
            The connections can be accepted using **ipc_accept** function.
        `,
        epilogue: `
            The socket can be closed either by **hclose** or **ipc_close**.
            Both ways are equivalent.
        `,

        allocates_handle: true,
        mem: "IPC_LISTENER_SIZE",

        errors: ["EINVAL"],
        custom_errors: {
            EACCES: "The process does not have appropriate privileges.",
            EADDRINUSE: "The specified address is already in use.",
            ELOOP: "A loop exists in symbolic links encountered during resolution of the pathname in address.",
            ENAMETOOLONG: "A component of a pathname exceeded **NAME_MAX** characters, or an entire pathname exceeded **PATH_MAX** characters.",
            ENOENT: "A component of the pathname does not name an existing file or the pathname is an empty string.",
            ENOTDIR: "A component of the path prefix of the pathname in address is not a directory.",
        },
    },
    {
        name: "mrecv",
        section: "Message sockets",
        info: "receives a message",
        result: {
            type: "ssize_t",
            success: "size of the received message, in bytes",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket.",
           },
           {
               name: "buf",
               type: "void*",
               info: "Buffer to receive the message to.",
           },
           {
               name: "len",
               type: "size_t",
               info: "Size of the buffer, in bytes.",
           },
        ],
        prologue: `
            This function receives message from a socket. It is a blocking
            operation that unblocks only after entire message is received.
            There is no such thing as partial receive. Either entire message
            is received or no message at all.
        `,

        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        errors: ["EINVAL", "EMSGSIZE"],
        custom_errors: {
            EPIPE: "Closed connection.",
        },

        example: `
            char msg[100];
            ssize_t msgsz = mrecv(s, msg, sizeof(msg), -1);
        `,
    },
    {
        name: "mrecvl",
        section: "Message sockets",
        info: "receives a message",
        result: {
            type: "ssize_t",
            success: "size of the received message, in bytes",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket.",
           },
        ],
        prologue: `
            This function receives message from a socket. It is a blocking
            operation that unblocks only after entire message is received.
            There is no such thing as partial receive. Either entire message
            is received or no message at all.
        `,

        epilogue: `
            If both **first** and **last** arguments are set to **NULL**
            the message is received and silently dropped. The function will
            still return the size of the message.
        `,

        has_handle_argument: true,
        has_deadline: true,
        has_iol: true,
        uses_connection: true,

        errors: ["EINVAL", "EMSGSIZE"],
        custom_errors: {
            EPIPE: "Closed connection.",
        },
    },
    {
        name: "msend",
        section: "Message sockets",
        info: "sends a message",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket to send the message to.",
           },
           {
               name: "buf",
               type: "const void*",
               info: "Message to send.",
           },
           {
               name: "len",
               type: "size_t",
               info: "Size of the message, in bytes.",
           },
        ],

        prologue: `
            This function sends a message to a socket. It is a blocking
            operation that unblocks only after entire message is sent. 
            There is no such thing as partial send. If a problem, including
            timeout, occurs while sending the message error is returned to the
            user and the socket cannot be used for sending from that point on.
        `,

        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        errors: ["EINVAL"],
        custom_errors: {
            EMSGSIZE: "The message is too long.",
            EPIPE: "Closed connection.",
        },

        example: `
            int rc = msend(s, "ABC", 3, -1);
        `,
    },
    {
        name: "msendl",
        section: "Message sockets",
        info: "sends a message",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket to send the message to.",
           },
        ],

        prologue: `
            This function sends a message to a socket. It is a blocking
            operation that unblocks only after entire message is sent. 
            There is no such thing as partial send. If a problem, including
            timeout, occurs while sending the message error is returned to the
            user and the socket cannot be used for sending from that point on.
        `,

        has_handle_argument: true,
        has_deadline: true,
        has_iol: true,
        uses_connection: true,

        errors: ["EINVAL"],
        custom_errors: {
            EMSGSIZE: "The message is too long.",
            EPIPE: "Closed connection.",
        }
    },
    {
        name: "msleep",
        section: "Deadlines",
        info: "waits until deadline expires",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        has_deadline: true,

        prologue: `
            This function blocks until the deadline expires or an error occurs.
        `,

        example: `
            int rc = msleep(now() + 1000);
            if(rc != 0) {
                perror("Cannot sleep");
                exit(1);
            }
            printf("Slept succefully for 1 second.\n");
        `,
    },
    {
        name: "now",
        section: "Deadlines",
        info: "get current time",
        result: {
            type: "int64_t",
            info: "Current time."
        },
        args: [
        ],

        prologue: `
            Returns current time, in milliseconds.

            The function is meant for creating deadlines. For example, a point
            of time one second from now can be expressed as **now() + 1000**.

            The following values have special meaning and cannot be returned by
            the function:

            * 0: Immediate deadline.
            * -1: Infinite deadline.
        `,

        example: `
            int result = chrecv(ch, &val, sizeof(val), now() + 1000);
            if(result == -1 && errno == ETIMEDOUT) {
                printf("One second elapsed without receiving a message.\\n");
            }
        `,
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
               info: "Handle of the underlying socket. It must be a " +
                     "bytestream protocol.",
           },
        ],
        protocol: pfx_protocol,
        prologue: `
            This function instantiates PFX protocol on top of the underlying
            protocol.
        `,
        epilogue: "The socket can be cleanly shut down using **pfx_detach** " +
                  "function.",

        has_handle_argument: true,
        allocates_handle: true,

        mem: "PFX_SIZE",

        custom_errors: {
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
        prologue: `
            This function does the terminal handshake and returns underlying
            socket to the user. The socket is closed even in the case of error.
        `,

        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        custom_errors: {
            ENOTSUP: "The handle is not a PFX protocol handle.",
        },
    },
    {
        name: "tcp_accept",
        info: "accepts an incoming TCP connection",

        result: {
            type: "int",
            success: "handle of the new connection",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "Socket created by **tcp_listen**.",
            },
            {
                name: "addr",
                type: "struct ipaddr*",
                info: "Out parameter. IP address of the connecting endpoint. Can be **NULL**.",
            },
        ],

        has_deadline: true,

        protocol: tcp_protocol,

        prologue: `
            This function accepts an incoming TCP connection.
        `,
        epilogue: `
            The socket can be cleanly shut down using **tcp_close** function.
        `,

        has_handle_argument: true,
        allocates_handle: true,
        mem: "TCP_SIZE",

        errors: ["ECANCELED"],
    },
    {
        name: "tcp_close",
        info: "closes TCP connection in an orderly manner",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "The TCP socket.",
            },
        ],

        has_deadline: true,

        protocol: tcp_protocol,

        prologue: `
            This function closes a TCP socket cleanly. Unlike **hclose** it lets
            the peer know that it is shutting down and waits till the peer
            acknowledged the shutdown. If this terminal handshake cannot be
            done it returns error. The socket is closed even in the case of
            error.

            It can also be used to close TCP listener socket in which case it's
            equivalent to calling **hclose**.
        `,

        has_handle_argument: true,
        custom_errors: {
            ECONNRESET: "Broken connection.",
        },
    },
    {
        name: "tcp_connect",
        info: "creates a connection to remote TCP endpoint ",

        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address to connect to.",
            },
        ],

        has_deadline: true,

        protocol: tcp_protocol,

        prologue: `
            This function creates a connection to a remote TCP endpoint.
        `,
        epilogue: `
            The socket can be cleanly shut down using **tcp_close** function.
        `,

        allocates_handle: true,
        mem: "TCP_SIZE",

        errors: ["EINVAL"],
        custom_errors: {
            ECONNREFUSED: "The target address was not listening for connections or refused the connection request.",
            ECONNRESET: "Remote host reset the connection request.",
            EHOSTUNREACH: "The destination host cannot be reached.",
            ENETDOWN: "The local network interface used to reach the destination is down.",
            ENETUNREACH: "No route to the network is present.",
        },

        example: `
            struct ipaddr addr;
            ipaddr_remote(&addr, "www.example.org", 80, 0, -1);
            int s = tcp_connect(&addr, -1);
            bsend(s, "ABC", 3, -1);
            char buf[3];
            brecv(s, buf, sizeof(buf), -1);
            tcp_close(s);
        `
    },
    {
        name: "tcp_listen",
        info: "starts listening for incoming TCP connections",

        result: {
            type: "int",
            success: "newly created socket",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address to listen on.",
            },
            {
                name: "backlog",
                type: "int",
                info: "Maximum number of connections that can be kept open without accepting them.",
            },
        ],

        protocol: tcp_protocol,

        prologue: `
            This function starts listening for incoming connections.
            The connections can be accepted using **tcp_accept** function.
        `,
        epilogue: `
            The socket can be closed either by **hclose** or **tcp_close**.
            Both ways are equivalent.
        `,

        allocates_handle: true,
        mem: "TCP_LISTENER_SIZE",

        errors: ["EINVAL"],
        custom_errors: {
            EADDRINUSE: "The specified address is already in use.",
            EADDRNOTAVAIL: "The specified address is not available from the local machine.",
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
               info: "Handle of the underlying socket. It must be a " +
                     "bytestream protocol.",
           },
        ],
        protocol: tls_protocol,
        prologue: `
            This function instantiates TLS protocol on top of the underlying
            protocol. TLS protocol being asymmetric, client and server sides are
            intialized in different ways. This particular function initializes
            the client side of the connection.
        `,
        epilogue: `
            The socket can be cleanly shut down using **tls_detach** function.
        `,
        has_handle_argument: true,
        has_deadline: true,
        allocates_handle: true,
        uses_connection: true,

        mem: "TLS_SIZE",

        custom_errors: {
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
               info: "Handle of the underlying socket. It must be a " +
                     "bytestream protocol.",
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
        prologue: `
            This function instantiates TLS protocol on top of the underlying
            protocol. TLS protocol being asymmetric, client and server sides are
            intialized in different ways. This particular function initializes
            the server side of the connection.
        `,
        epilogue: `
            The socket can be cleanly shut down using **tls_detach** function.
        `,
        has_handle_argument: true,
        has_deadline: true,
        allocates_handle: true,
        uses_connection: true,

        mem: "TLS_SIZE",

        errors: ["EINVAL"],

        custom_errors: {
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
        prologue: `
            This function does the terminal handshake and returns underlying
            socket to the user. The socket is closed even in the case of error.
        `,

        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        custom_errors: {
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
               info: "IP  address to be used to set source IP address in " +
                     "outgoing packets. Also, the socket will receive " +
                     "packets sent to this address. If port in the address " +
                     "is set to zero an ephemeral port will be chosen and " +
                     "filled into the local address.",
           },
           {
               name: "remote",
               type: "struct ipaddr*",
               info: "IP address used as default destination for outbound " +
                     "packets. It is used when destination address in " +
                     "**udp_send** function is set to **NULL**. It is also " +
                     "used by **msend** and **mrecv** functions which don't " +
                     "allow to specify the destination address explicitly.",
           },
        ],
        protocol: udp_protocol,
        prologue: "This function creates an UDP socket.",
        epilogue: "To close this socket use **hclose** function.",

        allocates_handle: true,
        mem: "UDP_SIZE",

        errors: ["EINVAL"],
        custom_errors: {
            EADDRINUSE: "The local address is already in use.",
            EADDRNOTAVAIL: "The specified address is not available from the " +
                           "local machine.",
        },
    },
    {
        name: "udp_recv",
        info: "receives an UDP packet",
        result: {
            type: "ssize_t",
            success: "size of the received message, in bytes",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the UDP socket.",
           },
           {
               name: "addr",
               type: "struct ipaddr*",
               info: "Out parameter. IP address of the sender of the packet. " +
                     "Can be **NULL**.",
           },
           {
               name: "buf",
               type: "void*",
               info: "Buffer to receive the data to.",
           },
           {
               name: "len",
               type: "size_t",
               info: "Size of the buffer, in bytes.",
           },
        ],
        protocol: udp_protocol,
        prologue: "This function receives a single UDP packet.",

        has_handle_argument: true,
        has_deadline: true,

        errors: ["EINVAL", "EMSGSIZE"],
    },
    {
        name: "udp_recvl",
        info: "receives an UDP packet",
        result: {
            type: "ssize_t",
            success: "size of the received message, in bytes",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the UDP socket.",
           },
           {
               name: "addr",
               type: "struct ipaddr*",
               info: "Out parameter. IP address of the sender of the packet. " +
                     "Can be **NULL**.",
           },
        ],
        protocol: udp_protocol,
        prologue: "This function receives a single UDP packet.",

        has_handle_argument: true,
        has_deadline: true,
        has_iol: true,

        errors: ["EINVAL", "EMSGSIZE"],
    },
    {
        name: "udp_send",
        info: "sends an UDP packet",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the UDP socket.",
           },
           {
               name: "addr",
               type: "const struct ipaddr*",
               info: "IP address to send the packet to. If set to **NULL** " +
                     "remote address specified in **udp_open** function will " +
                     "be used.",
           },
           {
               name: "buf",
               type: "const void*",
               info: "Data to send.",
           },
           {
               name: "len",
               type: "size_t",
               info: "Number of bytes to send.",
           },
        ],
        protocol: udp_protocol,
        prologue: `
            This function sends an UDP packet.

            Given that UDP protocol is unreliable the function has no deadline.
            If packet cannot be sent it will be silently dropped.
        `,

        has_handle_argument: true,
        has_deadline: false,

        errors: ["EINVAL", "EMSGSIZE"],
        custom_errors: {
            EMSGSIZE: "The message is too long to fit into an UDP packet.",
        }
    },
    {
        name: "udp_sendl",
        info: "sends an UDP packet",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the UDP socket.",
           },
           {
               name: "addr",
               type: "const struct ipaddr*",
               info: "IP address to send the packet to. If set to **NULL** " +
                     "remote address specified in **udp_open** function will " +
                     "be used.",
           },
        ],
        protocol: udp_protocol,
        prologue: `
            This function sends an UDP packet.

            Given that UDP protocol is unreliable the function has no deadline.
            If packet cannot be sent it will be silently dropped.
        `,

        has_handle_argument: true,
        has_iol: true,

        errors: ["EINVAL", "EMSGSIZE"],
        custom_errors: {
            EMSGSIZE: "The message is too long to fit into an UDP packet.",
        }
    },
    {
        name: "yield",
        section: "Coroutines",
        info: "yields CPU to other coroutines",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
        ],

        prologue: `
            By calling this function, you give other coroutines a chance to run.

            You should consider using **yield** when doing lengthy computations
            which don't have natural coroutine switching points such as socket
            or channel operations or msleep.
        `,

        errors: ["ECANCELED"],

        example: `
            for(i = 0; i != 1000000; ++i) {
                expensive_computation();
                yield(); /* Give other coroutines a chance to run. */
            }
        `,
    },
]

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
            type: "void*",
            info: "The memory to store the newly created object. It must be " +
                  "at least **" + fx.mem + "** bytes long and must not be " +
                  "deallocated before the object is closed.",
        }
        if(fx.name === "chmake") a.unshift(memarg)
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

    for(var j = 0; j < a.length; j++) {
        if(a[j].type) t += a[j].type + " "
        t += a[j].name
        if(a[j].postfix) t += a[j].postfix
        if(j != a.length - 1) t += ", "
    }
    if(a.length == 0 && !fx.has_iol && !mem && !fx.has_deadline) t += "void"
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

                void *iol_base;          /* Pointer to the buffer. */
                size_t iol_len;          /* Size of the buffer. */
                struct iolist *iol_next; /* Next buffer in the list. */
                int iol_rsvd;            /* Reserved. Must be set to zero. */

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
t += generate_section("TCP protocol", sections)
t += generate_section("IPC protocol", sections)
t += generate_section("UDP protocol", sections)
t += generate_section("TLS protocol", sections)
t += generate_section("CRLF protocol", sections)
t += generate_section("PFX protocol", sections)
t += generate_section("HTTP protocol", sections)
fs.writeFile("toc.md", t)

// Generate individual man pages.
for(var i = 0; i < fxs.length; i++) {
    fx = fxs[i];
    t = generate_man_page(fx, sections, false)
    fs.writeFile(fx.name + ".md", t)
    // Mem functions have no definitions of their own. The docs are generated
    // from the related non-mem function.
    if(fx.mem != undefined) {
        t = generate_man_page(fx, sections, true)
        fs.writeFile(fx.name + "_mem.md", t)
    }
 }

