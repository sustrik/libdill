
fxs.append(
    {
        "name": "ws_response_key",
        "section": "WebSocket protocol",
        "info": "generates a WebSocket response key for a given request key",
        "result": {
            "type": "int",
            "success": "0",
            "error": "-1",
        },
        "args": [
           {
               "name": "request_key",
               "type": "const char*",
               "info": "The request key, as passed in **Sec-WebSocket-Key** field.",
           },
           {
               "name": "response_key",
               "type": "char*",
               "info": """
                   Buffer to hold the generated response key. It must be at
                   least **WS_KEY_SIZE** bytes long.
               """,
           },
        ],

        "protocol": ws_protocol,

        "prologue": """
            This is a helper function that can be used when doing a custom HTTP
            handshake for the WebSocket protocol (see **WS_NOHTTP** flag).
            On the server, it can be used to  generates an RFC-compliant
            response key, to be filled into **Sec-WebSocket-Accept** field,
            for a request key received from the client. On the client side, it
            can be used to verify that the response key received from the server
            is correct.

            The generated key is null-terminated.
        """,

        "errors": ["EINVAL"],

        "custom_errors": {
            "EPROTO": "Supplied request key is not RFC 6455 compliant.",
        },

        "example": """
            char name[256];
            char value[256];
            http_recvfield(s, name, sizeof(name), value, sizeof(value), -1);
            assert(strcasecmp(name, "Sec-WebSocket-Key") == 0);
            ws_response_key(value, response_key);
            http_sendfield(s, "Sec-WebSocket-Accept", response_key, -1);
        """,
    }
)
