
fxs.append(
    {
        "name": "ipaddr_setport",
        "topic": "IP addresses",
        "info": "changes port number of the address",
        "args": [
            {
                "name": "addr",
                "type": "const struct ipaddr*",
                "dill": True,
                "info": "IP address object.",
            },
        ],

        "prologue": """
            Changes port number of the address.
        """,

        "example": """
            ipaddr_setport(&addr, 80);
        """,
    }
)
