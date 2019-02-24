
tcp_accept_function = {
    "name": "tcp_accept",
    "topic": "tcp",
    "info": "accepts an incoming TCP connection",

    "result": {
        "type": "int",
        "success": "handle of the new connection",
        "error": "-1",
    },
    "args": [
        {
            "name": "s",
            "type": "int",
            "info": "Socket created by **tcp_listen**.",
        },
        {
            "name": "opts",
            "type": "const struct tcp_opts*",
            "dill": True,
            "info": "Options.",
        },
        {
            "name": "addr",
            "type": "struct ipaddr*",
            "dill": True,
            "info": "Out parameter. IP address of the connecting endpoint. Can be **NULL**.",
        },
    ],

    "has_deadline": True,

    "prologue": """
        This function accepts an incoming TCP connection.
    """,
    "epilogue": """
        The socket can be cleanly shut down using **tcp_close** function.
    """,

    "has_handle_argument": True,
    "allocates_handle": True,
    "mem": "tcp_storage",

    "errors": ["ECANCELED"],
}

new_function(tcp_accept_function)

