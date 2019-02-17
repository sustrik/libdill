
newFunction(
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
    }
)
