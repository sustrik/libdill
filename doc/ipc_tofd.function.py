
ipc_tofd_function = {
    "name": "ipc_tofd",
    "topic": "ipc",
    "info": "returns the underlying OS-level file descriptor",

    "result": {
        "type": "int",
        "success": "the file descriptor",
        "error": "-1",
    },
    "args": [
        {
            "name": "s",
            "type": "int",
            "info": "Handle of an IPC socket.",
        },
    ],

    "prologue": """
        This function destroys an IPC socket object without closing
        the underlying connection. It returns the OS-level file
        descriptor of the connection.
    """,

    "custom_errors": {
        "ENOTSUP": "Either the handle doesn't point to a IPC socket or the IPC socket has RX buffering on.",
    },

}

new_function(ipc_tofd_function)

