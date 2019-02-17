
fxs.append(
    {
        "name": "tcp_done",
        "info": "half-closes a TCP connection",

        "result": {
            "type": "int",
            "success": "0",
            "error": "-1",
        },
        "args": [
            {
                "name": "s",
                "type": "int",
                "info": "The TCP connection handle.",
            },
        ],

        "has_deadline": True,

        "protocol": tcp_protocol,

        "prologue": """
            This function closes the outbound half of TCP connection.
            Technically, it sends a FIN packet to the peer. This will, in turn,
            cause the peer to get **EPIPE** error after it has received all
            the data. 
        """,

        "has_handle_argument": True,

        "custom_errors": {
            "EPIPE": "The connection was already half-closed.",
        },
    }
)
