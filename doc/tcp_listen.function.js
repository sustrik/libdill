
newFunction(
    {
        name: "tcp_listen",
        info: "starts listening for incoming TCP connections",

        result: {
            type: "int",
            success: "newly created socket",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address to listen on.",
            },
            {
                name: "backlog",
                type: "int",
                info: "Maximum number of connections that can be kept open without accepting them.",
            },
        ],

        protocol: tcp_protocol,

        prologue: `
            This function starts listening for incoming connections.
            The connections can be accepted using **tcp_accept** function.
        `,
        epilogue: `
            The socket can be closed either by **hclose** or **tcp_close**.
            Both ways are equivalent.
        `,

        allocates_handle: true,
        mem: "tcp_listener_storage",

        errors: ["EINVAL"],
        custom_errors: {
            EADDRINUSE: "The specified address is already in use.",
            EADDRNOTAVAIL: "The specified address is not available from the local machine.",
        },
    }
)
