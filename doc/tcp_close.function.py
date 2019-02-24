
tcp_close_function = {
    "name": "tcp_close",
    "topic": "tcp",
    "info": "closes TCP connection in an orderly manner",

    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },
    "args": [
        {
            "name": "s",
            "type": "int",
            "info": "The TCP socket.",
        },
    ],

    "has_deadline": True,

    "protocol": tcp_protocol,

    "prologue": """
        This function closes a TCP socket cleanly. Unlike **hclose** it lets
        the peer know that it is shutting down and waits till the peer
        acknowledged the shutdown. If this terminal handshake cannot be
        done it returns error. The socket is closed even in the case of
        error.

        It can also be used to close TCP listener socket in which case it's
        equivalent to calling **hclose**.
    """,

    "has_handle_argument": True,
    "custom_errors": {
        "ECONNRESET": "Broken connection.",
    },
}

new_function(tcp_close_function)

