
fxs.append(
    {
        "name": "http_recvstatus",
        "info": "receives HTTP status from the peer",
        "result": {
            "type": "int",
            "success": "HTTP status code, such as 200 or 404",
            "error": "-1",
        },
        "args": [
           {
               "name": "s",
               "type": "int",
               "info": "HTTP socket handle.",
           },
           {
               "name": "reason",
               "type": "char*",
               "info": "Buffer to store reason string in.",
           },
           {
               "name": "reasonlen",
               "type": "size_t",
               "info": "Size of the **reason** buffer.",
           },
        ],
        "protocol": http_protocol,
        "prologue": "This function receives an HTTP status line from the peer.",
        "has_handle_argument": True,
        "has_deadline": True,
        "uses_connection": True,

        "errors": ["EINVAL", "EMSGSIZE"],
    }
)
