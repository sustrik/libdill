
newFunction(
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
    }
)
