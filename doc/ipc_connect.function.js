
newFunction(
    {
        name: "ipc_connect",
        info: "creates a connection to remote IPC endpoint",

        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "const char*",
                info: "Filename to connect to.",
            },
        ],

        has_deadline: true,

        protocol: ipc_protocol,

        prologue: `
            This function creates a connection to a remote IPC endpoint.
        `,
        epilogue: `
            The socket can be cleanly shut down using **ipc_close** function.
        `,

        allocates_handle: true,
        mem: "ipc_storage",

        errors: ["EINVAL"],
        custom_errors: {
            EACCES: "The process does not have appropriate privileges.",
            ECONNREFUSED: "The target address was not listening for connections or refused the connection request.",
            ECONNRESET: "Remote host reset the connection request.",
            ELOOP: "A loop exists in symbolic links encountered during resolution of the pathname in address.",
            ENAMETOOLONG: "A component of a pathname exceeded **NAME_MAX** characters, or an entire pathname exceeded **PATH_MAX** characters.",
            ENOENT: "A component of the pathname does not name an existing file or the pathname is an empty string.",
            ENOTDIR: "A component of the path prefix of the pathname in address is not a directory.",
        },

        example: `
            int s = ipc_connect("/tmp/test.ipc", -1);
            bsend(s, "ABC", 3, -1);
            char buf[3];
            brecv(s, buf, sizeof(buf), -1);
            ipc_close(s);
        `
    }
)
