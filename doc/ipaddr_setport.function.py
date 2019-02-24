
ipaddr_setport_function = {
    "name": "ipaddr_setport",
    "topic": "ipaddr",
    "info": "changes port number of the address",
    "args": [
        {
            "name": "addr",
            "type": "struct ipaddr*",
            "dill": True,
            "info": "IP address object.",
        },
        {
            "name": "port",
            "type": "int",
            "info": "New port number.",
        },
    ],

    "prologue": """
        Changes port number of the address.
    """,

    "example": """
        ipaddr_setport(&addr, 80);
    """,
}

new_function(ipaddr_setport_function)
