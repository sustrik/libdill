
newFunction(
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
    }
)
