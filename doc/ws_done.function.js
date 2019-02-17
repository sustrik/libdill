
newFunction(
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
    }
)
