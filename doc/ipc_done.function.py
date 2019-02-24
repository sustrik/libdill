
ipc_done_function = {
    "name": "ipc_done",
    "topic": "ipc",
    "info": "half-closes a IPC connection",

    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },
    "args": [
        {
            "name": "s",
            "type": "int",
            "info": "The IPC connection handle.",
        },
    ],

    "has_deadline": True,

    "prologue": """
        This function closes the outbound half of ICP connection.
        This will cause the peer to get **EPIPE** error after it has
        received all the data. 
    """,

    "has_handle_argument": True,

    "custom_errors": {
        "EPIPE": "The connection was already half-closed.",
    },
}

new_function(ipc_done_function)

