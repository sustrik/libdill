
tcpmux_daemon_function = {
    "name": "tcpmux_daemon",
    "topic": "tcpmux",
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

    "prologue": """
        This function starts a TCPMUX daemon. The function doesn't return unless
        there's an error.
    """,

    "errors": [],
}

new_function(tcpmux_daemon_function)

