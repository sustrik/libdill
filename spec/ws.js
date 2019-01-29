fxs = [
    {
        name: "ws_attach_client",
        info: "creates WebSocket protocol on the top of underlying socket",
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
               name: "flags",
               type: "int",
               info: "Flags. See above.",
           },
           {
               name: "resource",
               type: "const char*",
               info: "HTTP resource to use.",
           },
           {
               name: "host",
               type: "const char*",
               info: "Virtual HTTP host to use.",
           },
        ],
        protocol: ws_protocol,
        prologue: `
            This function instantiates WebSocket protocol on top of the
            underlying bytestream protocol. WebSocket protocol being asymmetric,
            client and server sides are intialized in different ways. This
            particular function initializes the client side of the connection.
        ` + ws_flags,
        epilogue: `
            The socket can be cleanly shut down using **ws_detach** function.
        `,
        has_handle_argument: true,
        has_deadline: true,
        allocates_handle: true,
        uses_connection: true,

        mem: "ws_storage",

        errors: ["EINVAL"],

        custom_errors: {
            EPROTO: "Underlying socket is not a bytestream socket.",
        },
    },
    {
        name: "ws_attach_server",
        info: "creates WebSocket protocol on the top of underlying socket",
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
               name: "flags",
               type: "int",
               info: "Flags. See above.",
           },
           {
               name: "resource",
               type: "char*",
               info: "Out parameter. HTTP resource specified by the client.",
           },
           {
               name: "resourcelen",
               type: "size_t",
               info: "Size of the **resource** buffer.",
           },
           {
               name: "host",
               type: "char*",
               info: "Out parameter. Virtual host specified by the client.",
           },
           {
               name: "hostlen",
               type: "size_t",
               info: "Size of the **host** buffer.",
           },
        ],
        protocol: ws_protocol,
        prologue: `
            This function instantiates WebSocket protocol on top of the
            underlying bytestream protocol. WebSocket protocol being asymmetric,
            client and server sides are intialized in different ways. This
            particular function initializes the client side of the connection.
        ` + ws_flags,
        epilogue: `
            The socket can be cleanly shut down using **ws_detach** function.
        `,
        has_handle_argument: true,
        has_deadline: true,
        allocates_handle: true,
        uses_connection: true,

        mem: "ws_storage",

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
        name: "ws_detach",
        info: "terminates WebSocket protocol and returns the underlying socket",
        result: {
            type: "int",
            success: "underlying socket handle",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the WebSocket socket.",
           },
           {
               name: "status",
               type: "int",
               info: "Status to send to the peer. If set zero, no status will be sent.",
           },
           {
               name: "buf",
               type: "const void*",
               info: "Message to send to the peer. If set to NULL, no message " +
                     "will be sent. Note that WebSocket protocol limits the size" +
                     "of the shutdown message to 125 bytes.",
           },
           {
               name: "len",
               type: "size_t",
               info: "Size of the message to send to the peer, in bytes.",
           },
        ],
        protocol: ws_protocol,
        prologue: `
            This function does the terminal WebSocket handshake and returns
            the underlying socket to the user. The socket is closed even in the
            case of error.
        `,

        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        custom_errors: {
            ENOTSUP: "The handle is not an WebSocket protocol handle.",
        },
    },
    {
        name: "ws_done",
        info: "half-closes a WebSocket connection",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "The WebSocket connection handle.",
            },
            {
                name: "status",
                type: "int",
                info: "Status to send to the peer. If set zero, no status will be sent.",
            },
            {
                name: "buf",
                type: "const void*",
                info: "Message to send to the peer. If set to NULL, no message " +
                      "will be sent. Note that WebSocket protocol limits the size" +
                      "of the shutdown message to 125 bytes.",
            },
            {
                name: "len",
                type: "size_t",
                info: "Size of the message to send to the peer, in bytes.",
            },
        ],

        has_deadline: true,

        protocol: ws_protocol,

        prologue: `
            This function closes the outbound half of a WebSocket connection.
            This will, in turn, cause the peer to get **EPIPE** error after it
            has received all the data. 
        `,

        has_handle_argument: true,

        custom_errors: {
            EPIPE: "The connection was already half-closed.",
        },
    },
    {
        name: "ws_recv",
        info: "receives a WebSocket message",
        result: {
            type: "ssize_t",
            success: "size of the received message, in bytes",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket handle.",
           },
           {
               name: "flags",
               type: "int*",
               info: "Out parameter. Possible values are either **WS_BINARY**" +
                     "or **WS_TEXT**.",
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
        protocol: ws_protocol,
        prologue: `
            This function receives one WebSocket message. It is very much the
            same as **mrecv** except that it returns the type of the message
            (either text or binary).
        `,

        has_handle_argument: true,
        uses_connection: true,
        has_deadline: true,

        errors: ["EINVAL", "EBUSY", "EMSGSIZE"],
    },
    {
        name: "ws_recvl",
        info: "receives a WebSocket message",
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
               name: "flags",
               type: "int*",
               info: "Out parameter. Possible values are either **WS_BINARY**" +
                     "or **WS_TEXT**.",
           },
        ],
        protocol: ws_protocol,
        prologue: `
            This function receives one WebSocket message. It is very much the
            same as **mrecvl** except that it returns the type of the message
            (either text or binary).
        `,

        has_handle_argument: true,
        uses_connection: true,
        has_deadline: true,
        has_iol: true,

        errors: ["EINVAL", "EBUSY", "EMSGSIZE"],
    },
    {
        name: "ws_request_key",
        section: "WebSocket protocol",
        info: "generates an unique value for Sec-WebSocket-Key field",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "request_key",
               type: "char*",
               info: `
                   Buffer to hold the generated request key. It must be at least
                   **WS_KEY_SIZE** bytes long.
               `,
           },
        ],

        protocol: ws_protocol,

        prologue: `
            This is a helper function that can be used when doing a custom HTTP
            handshake for the WebSocket protocol (see **WS_NOHTTP** flag).
            It generates an unique RFC-compliant key to be filled into
            **Sec-WebSocket-Key** of the HTTP request.

            The generated key is null-terminated.
        `,

        errors: ["EINVAL"],

        example: `
            char request_key[WS_KEY_SIZE];
            ws_request_key(request_key);
            http_sendfield(s, "Sec-WebSocket-Key", request_key, -1);
        `,
    },
    {
        name: "ws_response_key",
        section: "WebSocket protocol",
        info: "generates a WebSocket response key for a given request key",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "request_key",
               type: "const char*",
               info: "The request key, as passed in **Sec-WebSocket-Key** field.",
           },
           {
               name: "response_key",
               type: "char*",
               info: `
                   Buffer to hold the generated response key. It must be at
                   least **WS_KEY_SIZE** bytes long.
               `,
           },
        ],

        protocol: ws_protocol,

        prologue: `
            This is a helper function that can be used when doing a custom HTTP
            handshake for the WebSocket protocol (see **WS_NOHTTP** flag).
            On the server, it can be used to  generates an RFC-compliant
            response key, to be filled into **Sec-WebSocket-Accept** field,
            for a request key received from the client. On the client side, it
            can be used to verify that the response key received from the server
            is correct.

            The generated key is null-terminated.
        `,

        errors: ["EINVAL"],

        custom_errors: {
            EPROTO: "Supplied request key is not RFC 6455 compliant.",
        },

        example: `
            char name[256];
            char value[256];
            http_recvfield(s, name, sizeof(name), value, sizeof(value), -1);
            assert(strcasecmp(name, "Sec-WebSocket-Key") == 0);
            ws_response_key(value, response_key);
            http_sendfield(s, "Sec-WebSocket-Accept", response_key, -1);
        `,
    },
    {
        name: "ws_send",
        info: "sends a WebSocket message",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket handle.",
           },
           {
               name: "flags",
               type: "int",
               info: `
                   Type of message to send. Either **WS_BINARY** or **WS_TEXT**.
                   This will override the type specified when attaching the
                   socket.
               `,
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
        protocol: ws_protocol,
        prologue: `
            This function works very much like **msend** except that it allows
            message type (text or binary) to be specified explicitly rather
            that using the type specified at attach time.
        `,

        has_handle_argument: true,
        uses_connection: true,
        has_deadline: true,

        errors: ["EINVAL", "EBUSY"],
        custom_errors: {
            EPIPE: "Closed connection.",
        },
    },
    {
        name: "ws_sendl",
        info: "sends a WebSocket message",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket handle.",
           },
           {
               name: "flags",
               type: "int",
               info: `
                   Type of message to send. Either **WS_BINARY** or **WS_TEXT**.
                   This will override the type specified when attaching the
                   socket.
               `,
           },
        ],
        protocol: ws_protocol,
        prologue: `
            This function works very much like **msendl** except that it allows
            message type (text or binary) to be specified explicitly rather
            that using the type specified at attach time.
        `,

        has_handle_argument: true,
        uses_connection: true,
        has_iol: true,
        has_deadline: true,

        errors: ["EINVAL", "EBUSY"],
        custom_errors: {
            EPIPE: "Closed connection.",
        },
    },
    {
        name: "ws_status",
        info: "retrieves the status after protocol termination",
        result: {
            type: "ssize_t",
            success: "length of the terminal message",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket handle.",
           },
           {
               name: "status",
               type: "int*",
               info: "Out parameter. Status number sent by the peer. If zero, " +
                     "there was no status.",
           },
           {
               name: "buf",
               type: "void*",
               info: "Buffer to fill the terminal message into. If set to " +
                     "**NULL** the message won't be returned.",           
           },
           {
               name: "len",
               type: "size_t",
               info: "Size of the buffer, in bytes.",           
           },
        ],
        protocol: ws_protocol,
        prologue: `
            When peer shuts down the protocol, receiving functions will start
            failing with **EPIPE** error. Once that happens **ws_status**
            function allows to retrieve the status and message sent by the
            peer when it closed the protocol.
        `,

        has_handle_argument: true,
        errors: ["EINVAL"],
        custom_errors: {
            EAGAIN: "The connection wasn't yet terminated by the peer.",
            EMSGSIZE: "The message doesn't fit in the supplied buffer.",
        },
    },
]
