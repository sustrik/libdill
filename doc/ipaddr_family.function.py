
fxs.append(
    {
        "name": "ipaddr_family",
        "topic": "IP addresses",
        "info": "returns family of the IP address",
        "result": {
            "type": "int",
            "info": "IP family.",
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
            Returns family of the address, i.e. either AF_INET for IPv4
            addresses or AF_INET6 for IPv6 addresses.
        """,

        "example": ipaddr_example,
    }
)
