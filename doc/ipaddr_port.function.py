
fxs.append(
    {
        "name": "ipaddr_port",
        "topic": "IP addresses",
        "info": "returns the port part of the address",
        "result": {
            "type": "int",
            "info": "The port number.",
        },
        "args": [
            {
                "name": "addr",
                "type": "const struct ipaddr*",
                "dill": True,
                "info": "IP address object.",
            },
        ],

        "prologue": """
            Returns the port part of the address.
        """,

        "example": """
            int port = ipaddr_port(&addr);
        """,
    }
)
