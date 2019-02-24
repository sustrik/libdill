
ipc_pair_function = {
    "name": "ipc_pair",
    "topic": "ipc",
    "info": "creates a pair of mutually connected IPC sockets",

    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },

    "args": [
        {
            "name": "s",
            "type": "int",
            "suffix": "[2]",
            "info": "Out parameter. Two handles to the opposite ends of the connection."
        },
        {
            "name": "opts1",
            "type": "const struct ipc_opts*",
            "dill": True,
            "info": "Options for the first socket."
        },
        {
            "name": "opts2",
            "type": "const struct ipc_opts*",
            "dill": True,
            "info": "Options for the second socket."
        },
    ],

    "prologue": """
        This function creates a pair of mutually connected IPC sockets.
    """,
    "epilogue": """
        The sockets can be cleanly shut down using **ipc_close** function.
    """,

    "allocates_handle": True,

    "errors": ['ECANCELED'],

    "example": """
          int s[2];
          int rc = ipc_pair(s);
    """,

    "mem": "ipc_pair_storage",
}

new_function(ipc_pair_function)
