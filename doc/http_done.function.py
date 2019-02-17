
fxs.append(
    {
        "name": "http_done",
        "info": "half-closes a HTTP connection",

        "result": {
            "type": "int",
            "success": "0",
            "error": "-1",
        },
        "args": [
            {
                "name": "s",
                "type": "int",
                "info": "The HTTP connection handle.",
            },
        ],

        "has_deadline": True,

        "protocol": http_protocol,

        "prologue": """
            This function closes the outbound half of an HTTP connection.
            Technically, it sends a blank line to the peer. This will, in turn,
            cause the peer to get **EPIPE** error after it has received all
            the data.

            Note that HTTP servers rely on getting a blank line before they
            send a response to a request. Therefore, client, after sending
            all the fields, must use **http_done** to inform the server that
            it should act upon the request.
        """,

        "has_handle_argument": True,

        "custom_errors": {
            "EPIPE": "The connection was already half-closed.",
        },
    }
)
