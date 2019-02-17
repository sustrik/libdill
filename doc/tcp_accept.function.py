
fxs.append(
    {
        "name": "tcp_accept",
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
                "name": "addr",
                "type": "struct ipaddr*",
                "info": "Out parameter. IP address of the connecting endpoint. Can be **NULL**.",
            },
        ],

        "has_deadline": True,

        "protocol": tcp_protocol,

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
)
