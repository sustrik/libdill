
fxs.append(
    {
        "name": "ipaddr_equal",
        "topic": "IP addresses",
        "info": "compares two IP address",
        "result": {
            "type": "int",
            "info": "1 if the arguments are equal, 0 otherwise.",
        },
        "args": [
            {
                "name": "addr1",
                "type": "const struct ipaddr*",
                "dill": True,
                "info": "First IP address.",
            },
            {
                "name": "addr2",
                "type": "const struct ipaddr*",
                "dill": True,
                "info": "Second IP address.",
            },
            {
                "name": "ignore_port",
                "type": "int",
                "info": "If set to zero addresses with different ports will be " +
                      "considered unequal. Otherwise, ports won't be taken " +
                      "into account.",
            },
        ],

        "prologue": """
            This function returns 1 is the two supplied IP addresses are
            the same or zero otherwise.
        """,

        "example": ipaddr_example,
    }
)
