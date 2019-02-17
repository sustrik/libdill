
newFunction(
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
    }
)
