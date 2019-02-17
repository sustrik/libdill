
newFunction(
    {
        name: "ipc_done",
        info: "half-closes a IPC connection",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "The IPC connection handle.",
            },
        ],

        has_deadline: true,

        protocol: ipc_protocol,

        prologue: `
            This function closes the outbound half of ICP connection.
            This will cause the peer to get **EPIPE** error after it has
            received all the data. 
        `,

        has_handle_argument: true,

        custom_errors: {
            EPIPE: "The connection was already half-closed.",
        },
    }
)
