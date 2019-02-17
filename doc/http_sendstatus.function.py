
fxs.append(
    {
        "name": "http_sendstatus",
        "info": "sends HTTP status to the peer",
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
               "name": "status",
               "type": "int",
               "info": "HTTP status such as 200 or 404.",
           },
           {
               "name": "reason",
               "type": "const char*",
               "info": "Reason string such as 'OK' or 'Not found'.",
           },
        ],
        "protocol": http_protocol,
        "prologue": """
            This function sends an HTTP status line to the peer. It is meant to
            be done at the beginning of the HTTP response. For example, if
            status is 404 and reason is 'Not found' the line sent will look like
            "this":

            ```
            HTTP/1.1 404 Not found
            ```
        """,
        "has_handle_argument": True,
        "has_deadline": True,
        "uses_connection": True,

        "errors": ["EINVAL"],

        "example": http_server_example,
    }
)
