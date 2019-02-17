
newFunction(
    {
        name: "ipc_accept",
        info: "accepts an incoming IPC connection",

        result: {
            type: "int",
            success: "handle of the new connection",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "Socket created by **ipc_listen**.",
            },
        ],

        has_deadline: true,

        protocol: ipc_protocol,

        prologue: `
            This function accepts an incoming IPC connection.
        `,
        epilogue: `
            The socket can be cleanly shut down using **ipc_close** function.
        `,

        has_handle_argument: true,
        allocates_handle: true,
        mem: "ipc_storage",
    }
)
