
ipc_accept_function = {
    "name": "ipc_accept",
    "topic": "ipc",
    "info": "accepts an incoming IPC connection",

    "result": {
        "type": "int",
        "success": "handle of the new connection",
        "error": "-1",
    },
    "args": [
        {
            "name": "s",
            "type": "int",
            "info": "Socket created by **ipc_listen**.",
        },
        {
            "name": "opts",
            "type": "const struct ipc_opts*",
            "dill": True,
            "info": "Options.",
        },
    ],

    "has_deadline": True,

    "prologue": """
        This function accepts an incoming IPC connection.
    """,
    "epilogue": """
        The socket can be cleanly shut down using **ipc_close** function.
    """,

    "has_handle_argument": True,
    "allocates_handle": True,
    "mem": "ipc_storage",
}

new_function(ipc_accept_function)

