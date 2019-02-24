
tls_done_function = {
    "name": "tls_done",
    "topic": "tls",
    "info": "half-closes a TLS protocol",

    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },
    "args": [
        {
            "name": "s",
            "type": "int",
            "info": "The TLS connection handle.",
        },
    ],

    "has_deadline": True,

    "prologue": """
        This function closes the outbound half of TLS connection. This will,
        in turn, cause the peer to get **EPIPE** error after it has received
        all the data. 
    """,

    "has_handle_argument": True,

    "custom_errors": {
        "EPIPE": "The connection was already half-closed.",
    },
}

new_function(tls_done_function)
