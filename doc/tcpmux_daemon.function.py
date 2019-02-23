
fxs.append(
    {
        "name": "tcpmux_daemon",
        "info": "runs TCPMUX daemon",

        "result": {
            "type": "int",
            "success": "0",
            "error": "-1",
        },
        "args": [
            {
                "name": "opts",
                "type": "const struct tcpmux_opts*",
                "dill": True,
                "info": "Options.",
            },
        ],

        "protocol": tcpmux_protocol,

        "prologue": """
            This function starts a TCPMUX daemon. The function doesn't return unless
            there's an error.
        """,

        "errors": [],
    }
)
