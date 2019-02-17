
fxs.append(
    {
        "name": "ipc_listen",
        "info": "starts listening for incoming IPC connections",

        "result": {
            "type": "int",
            "success": "newly created socket",
            "error": "-1",
        },
        "args": [
            {
                "name": "addr",
                "type": "const char*",
                "info": "The filename to listen on.",
            },
            {
                "name": "backlog",
                "type": "int",
                "info": "Maximum number of connections that can be kept open without accepting them.",
            },
        ],

        "protocol": ipc_protocol,

        "prologue": """
            This function starts listening for incoming IPC connections.
            The connections can be accepted using **ipc_accept** function.
        """,
        "epilogue": """
            The socket can be closed either by **hclose** or **ipc_close**.
            Both ways are equivalent.
        """,

        "allocates_handle": True,
        "mem": "ipc_listener_storage",

        "errors": ["EINVAL"],
        "custom_errors": {
            "EACCES": "The process does not have appropriate privileges.",
            "EADDRINUSE": "The specified address is already in use.",
            "ELOOP": "A loop exists in symbolic links encountered during resolution of the pathname in address.",
            "ENAMETOOLONG": "A component of a pathname exceeded **NAME_MAX** characters, or an entire pathname exceeded **PATH_MAX** characters.",
            "ENOENT": "A component of the pathname does not name an existing file or the pathname is an empty string.",
            "ENOTDIR": "A component of the path prefix of the pathname in address is not a directory.",
        },
    }
)
