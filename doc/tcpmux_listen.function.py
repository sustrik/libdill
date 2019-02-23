
fxs.append(
    {
        "name": "tcpmux_listen",
        "info": "starts listening for incoming TCPMUX connections",

        "result": {
            "type": "int",
            "success": "newly created socket",
            "error": "-1",
        },
        "args": [
            {
                "name": "service",
                "type": "const char*",
                "info": "Name of the service to listen on.",
            },
            {
                "name": "opts",
                "type": "const struct tcpmux_opts*",
                "dill": True,
                "info": "Options.",
            },
        ],

        "protocol": tcpmux_protocol,

        "prologue": """
            This function starts listening for incoming TCPMUX connections.
            The connections can be accepted using **tcpmux_accept** function.
        """,

        "has_deadline": True,
        "allocates_handle": True,

        "errors": [],
    }
)
