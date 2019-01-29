fxs = [
    {
        name: "suffix_attach",
        info: "creates SUFFIX protocol on top of underlying socket",
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
               name: "suffix",
               type: "const void*",
               info: "The delimiter byte sequence.",
           },
           {
               name: "suffixlen",
               type: "size_t",
               info: "The size of the delimiter, in bytes.",
           },
        ],
        protocol: suffix_protocol,
        prologue: `
            This function instantiates SUFFIX protocol on top of the underlying
            protocol.
        `,
        epilogue: `
            The socket can be cleanly shut down using **suffix_detach** function.
        `,
        has_handle_argument: true,
        allocates_handle: true,
        mem: "suffix_storage",
        custom_errors: {
            EPROTO: "Underlying socket is not a bytestream socket.",
        },
    },
    {
        name: "suffix_detach",
        info: "terminates SUFFIX protocol and returns the underlying socket",

        result: {
            type: "int",
            success: "underlying socket handle",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the SUFFIX socket.",
           },
        ],
        protocol: suffix_protocol,
        prologue: `
            This function does the terminal handshake and returns underlying
            socket to the user. The socket is closed even in the case of error.
        `,
        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        custom_errors: {
            ENOTSUP: "The handle is not a SUFFIX protocol handle.",
        },
    },
]
