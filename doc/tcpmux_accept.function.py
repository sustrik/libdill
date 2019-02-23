
fxs.append(
    {
        "name": "tcpmux_accept",
        "info": "accepts an incoming incoming TCPMUX connection",

        "result": {
            "type": "int",
            "success": "accepted socket",
            "error": "-1",
        },
        "args": [
            {
                "name": "s",
                "type": "int",
                "info": "The listening socket.",
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

        "protocol": tcpmux_protocol,

        "prologue": """
            This function accepts a TCP connection dispatched by TCPMUX daemon.
        """,
        "epilogue": """
            The socket can be cleanly shut down using **tcp_close** function.
        """,

        "has_deadline": True,
        "allocates_handle": True,
        "has_handle_argument": True,

        "errors": [],
    }
)
