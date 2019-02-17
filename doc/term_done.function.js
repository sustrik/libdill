
newFunction(
    {
        name: "term_done",
        info: "half-closes the connection",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The TERM protocol handle.",
           },
        ],
        protocol: term_protocol,
        prologue: `
            This function sends the termination message to the peer. Afterwards,
            it is not possible to send more messages. However, it is still
            possible to receiving outstanding inbound messages.
        `,
        has_handle_argument: true,
        has_deadline: true,

        custom_errors: {
            EIPE: "The termination message was already sent.",
            ENOTSUP: "The handle is not a TERM protocol handle.",
        },
    }
)
