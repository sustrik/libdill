
tcpmux_switch_function = {
    "name": "tcpmux_switch",
    "topic": "tcpmux",
    "info": "asks TCPMUX daemon to dispatch the connection to a service",

    "result": {
        "type": "int",
        "success": "accepted socket",
        "error": "-1",
    },
    "args": [
        {
            "name": "s",
            "type": "int",
            "info": "TCP socket.",
        },
        {
            "name": "service",
            "type": "const char*",
            "info": "Name of the service to switch to.",
        },
    ],

    "prologue": """
        Given an open connection to TCPMUX daemon, this function asks the daemon
        to dispatch the connection to the application listening on the specified
        service name.
    """,

    "has_deadline": True,
    "allocates_handle": True,

    "errors": [],
}

new_topic(tcpmux_switch_function)

