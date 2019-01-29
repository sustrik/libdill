fxs = [
    {
        name: "prefix_attach",
        info: "creates PREFIX protocol on top of underlying socket",
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
               name: "hdrlen",
               type: "size_t",
               info: "Size of the length field, in bytes."
           },
           {
               name: "flags",
               type: "int",
               info: "If set to **PREFIX_BIG_ENDIAN** (also known as network " +
                     "byte order, the default option) the most significant " +
                     "byte of the size will be sent first on the wire. If " +
                     "set to **PREFIX_LITTLE_ENDIAN** the least signiticant " +
                     "byte will come first.",
           },
        ],
        protocol: prefix_protocol,
        prologue: `
            This function instantiates PREFIX protocol on top of the underlying
            protocol.
        `,
        epilogue: "The socket can be cleanly shut down using **prefix_detach** " +
                  "function.",

        has_handle_argument: true,
        allocates_handle: true,

        mem: "prefix_storage",

        errors: ['EINVAL'],
        custom_errors: {
            EPROTO: "Underlying socket is not a bytestream socket.",
        },
    },
    {
        name: "prefix_detach",
        info: "terminates PREFIX protocol and returns the underlying socket",
        result: {
            type: "int",
            success: "underlying socket handle",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the PREFIX socket.",
           },
        ],
        protocol: prefix_protocol,
        prologue: `
            This function does the terminal handshake and returns underlying
            socket to the user. The socket is closed even in the case of error.
        `,

        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        custom_errors: {
            ENOTSUP: "The handle is not a PREFIX protocol handle.",
        },
    },
]
