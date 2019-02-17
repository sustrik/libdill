
fxs.append(
    {
        "name": "ipaddr_len",
        "section": "IP addresses",
        "info": "returns length of the address",
        "result": {
            "type": "int",
            "info": "Length of the IP address.",
        },
        "args": [
            {
                "name": "addr",
                "type": "const struct ipaddr*",
                "info": "IP address object.",
            },
        ],

        "prologue": """
            Returns lenght of the address, in bytes. This function is typically
            used in combination with **ipaddr_sockaddr** to pass address and its
            length to POSIX socket APIs.
        """,

        "example": ipaddr_example,
    }
)
