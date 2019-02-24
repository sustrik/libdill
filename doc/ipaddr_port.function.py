
ipaddr_port_function = {
    "name": "ipaddr_port",
    "topic": "ipaddr",
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

new_function(ipaddr_port_function)

