
fxs.append(
    {
        "name": "http_recvrequest",
        "info": "receives HTTP request from the peer",
        "result": {
            "type": "int",
            "success": "0",
            "error": "-1",
        },
        "args": [
           {
               "name": "s",
               "type": "int",
               "info": "HTTP socket handle.",
           },
           {
               "name": "command",
               "type": "char*",
               "info": "Buffer to store HTTP command in.",
           },
           {
               "name": "commandlen",
               "type": "size_t",
               "info": "Size of the **command** buffer.",
           },
           {
               "name": "resource",
               "type": "char*",
               "info": "Buffer to store HTTP resource in.",
           },
           {
               "name": "resourcelen",
               "type": "size_t",
               "info": "Size of the **resource** buffer.",
           },
        ],
        "protocol": http_protocol,
        "prologue": "This function receives an HTTP request from the peer.",
        "has_handle_argument": True,
        "has_deadline": True,
        "uses_connection": True,

        "errors": ["EINVAL", "EMSGSIZE"],

        "example": http_server_example,
    }
)
