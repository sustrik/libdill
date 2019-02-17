
newFunction(
    {
        name: "ws_attach_client",
        info: "creates WebSocket protocol on the top of underlying socket",
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
               name: "flags",
               type: "int",
               info: "Flags. See above.",
           },
           {
               name: "resource",
               type: "const char*",
               info: "HTTP resource to use.",
           },
           {
               name: "host",
               type: "const char*",
               info: "Virtual HTTP host to use.",
           },
        ],
        protocol: ws_protocol,
        prologue: `
            This function instantiates WebSocket protocol on top of the
            underlying bytestream protocol. WebSocket protocol being asymmetric,
            client and server sides are intialized in different ways. This
            particular function initializes the client side of the connection.
        ` + ws_flags,
        epilogue: `
            The socket can be cleanly shut down using **ws_detach** function.
        `,
        has_handle_argument: true,
        has_deadline: true,
        allocates_handle: true,
        uses_connection: true,

        mem: "ws_storage",

        errors: ["EINVAL"],

        custom_errors: {
            EPROTO: "Underlying socket is not a bytestream socket.",
        },
    }
)
