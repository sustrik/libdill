
ipc_recvfd_function = {
    "name": "ipc_recvfd",
    "topic": "ipc",
    "info": "receives a file descriptor from an IPC connection",

    "result": {
        "type": "int",
        "success": "the received file descriptor",
        "error": "-1",
    },
    "args": [
        {
            "name": "s",
            "type": "int",
            "info": "IPC connection handle.",
        },
    ],

    "prologue": """
        This function receives an OS-level file descriptor sent by
        ipc_sendfd function.

        Technically, the file descriptor is sent via SCM_RIGHTS option.
        The descriptor is attached to a message 1 byte long.
    """,

    "has_deadline": True,
}

new_function(ipc_recvfd_function)
