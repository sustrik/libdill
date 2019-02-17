
newFunction(
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
        mem: "http_storage",

        custom_errors: {
            EPROTO: "Underlying socket is not a bytestream socket.",
        },
    }
)
