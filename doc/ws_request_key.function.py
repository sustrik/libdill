
ws_request_key_function = {
    "name": "ws_request_key",
    "topic": "ws",
    "info": "generates an unique value for Sec-WebSocket-Key field",
    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },
    "args": [
       {
           "name": "request_key",
           "type": "char*",
           "info": """
               Buffer to hold the generated request key. It must be at least
               **WS_KEY_SIZE** bytes long.
           """,
       },
    ],

    "prologue": """
        This is a helper function that can be used when doing a custom HTTP
        handshake for the WebSocket protocol (see **WS_NOHTTP** flag).
        It generates an unique RFC-compliant key to be filled into
        **Sec-WebSocket-Key** of the HTTP request.

        The generated key is null-terminated.
    """,

    "errors": ["EINVAL"],

    "example": """
        char request_key[WS_KEY_SIZE];
        ws_request_key(request_key);
        http_sendfield(s, "Sec-WebSocket-Key", request_key, -1);
    """,
}

new_function(ws_request_key_function)

