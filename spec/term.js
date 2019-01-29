fxs = [
    {
        name: "term_attach",
        info: "creates TERM protocol on top of underlying socket",
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
                     "message protocol.",
           },
           {
               name: "buf",
               type: "const void*",
               info: "The terminal message.",
           },
           {
               name: "len",
               type: "size_t",
               info: "Size of the terminal message, in bytes.",
           },
        ],
        protocol: term_protocol,
        prologue: `
            This function instantiates TERM protocol on top of the underlying
            protocol.
        `,
        epilogue: `
            The socket can be cleanly shut down using **term_detach** function.
        `,
        has_handle_argument: true,
        has_deadline: true,
        allocates_handle: true,

        mem: "term_storage",

        custom_errors: {
            EPROTO: "Underlying socket is not a message socket.",
        },
    },
    {
        name: "term_detach",
        info: "cleanly terminates the protocol",
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
            This function cleanly terminates TERM protocol. If termination
            message wasn't yet sent to the peer using **term_done** function
            it will be sent now. Afterwards, any outstanding inbound messages
            will be received and dropped silently. The function returns
            after termination message from the peer is received.
        `,
        has_handle_argument: true,
        has_deadline: true,
        custom_errors: {
            ENOTSUP: "The handle is not a TERM protocol handle.",
        },
    },
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
            EPIPE: "The termination message was already sent.",
            ENOTSUP: "The handle is not a TERM protocol handle.",
        },
    },
]
