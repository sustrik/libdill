
tcp_listen_function = {
    "name": "tcp_listen",
    "topic": "tcp",
    "info": "starts listening for incoming TCP connections",

    "result": {
        "type": "int",
        "success": "newly created socket",
        "error": "-1",
    },
    "args": [
        {
            "name": "addr",
            "type": "struct ipaddr*",
            "dill": True,
            "info": "IP address to listen on.",
        },
        {
            "name": "opts",
            "type": "const struct tcp_opts*",
            "dill": True,
            "info": "Options.",
        },
    ],

    "prologue": """
        This function starts listening for incoming connections.
        The connections can be accepted using **tcp_accept** function.
    """,
    "epilogue": """
        The socket can be closed either by **hclose** or **tcp_close**.
        Both ways are equivalent.
    """,

    "allocates_handle": True,
    "mem": "tcp_listener_storage",

    "errors": ["EINVAL"],
    "custom_errors": {
        "EADDRINUSE": "The specified address is already in use.",
        "EADDRNOTAVAIL": "The specified address is not available from the local machine.",
    },
}

new_function(tcp_listen_function)

