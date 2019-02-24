
ipaddr_sockaddr_function = {
    "name": "ipaddr_sockaddr",
    "topic": "ipaddr",
    "info": "returns sockaddr structure corresponding to the IP address",
    "result": {
        "type": "const struct sockaddr*",
        "info": "Pointer to **sockaddr** structure correspoding to the address object.",
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
        Returns **sockaddr** structure corresponding to the IP address.
        This function is typically used in combination with ipaddr_len to
        pass address and its length to POSIX socket APIs.
    """,

    "example": ipaddr_example,
}

new_function(ipaddr_sockaddr_function)

