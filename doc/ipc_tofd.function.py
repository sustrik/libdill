
fxs.append(
    {
        "name": "ipc_tofd",
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

        "protocol": ipc_protocol,

        "prologue": """
            This function destroys an IPC socket object without closing
            the underlying connection. It returns the OS-level file
            descriptor of the connection.
        """,

        "custom_errors": {
            "ENOTSUP": "Either the handle doesn't point to a IPC socket or the IPC socket has RX buffering on.",
        },

    }
)
