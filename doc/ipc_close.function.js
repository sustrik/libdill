
newFunction(
    {
        name: "ipc_close",
        info: "closes IPC connection in an orderly manner",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "The IPC socket.",
            },
        ],

        has_deadline: true,

        protocol: ipc_protocol,

        prologue: `
            This function closes a IPC socket cleanly. Unlike **hclose** it lets
            the peer know that it is shutting down and waits till the peer
            acknowledged the shutdown. If this terminal handshake cannot be
            done it returns error. The socket is closed even in the case of
            error.

            It can also be used to close IPC listener socket in which case it's
            equivalent to calling **hclose**.
        `,

        has_handle_argument: true,
        custom_errors: {
            ECONNRESET: "Broken connection.",
        },
    }
)
