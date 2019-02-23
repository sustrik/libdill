
fxs.append(
    {
        "name": "tcp_tofd",
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
                "info": "Handle of a TCP socket.",
            },
        ],

        "protocol": tcp_protocol,

        "prologue": """
            This function destroys a TCP socket object without closing
            the underlying connection. It returns the OS-level file
            descriptor of the connection.
        """,

        "custom_errors": {
            "ENOTSUP": "Either the handle doesn't point to a TCP socket or the TCP socket has RX buffering on.",
        },

    }
)
