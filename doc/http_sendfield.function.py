
http_sendfield_function = {
    "name": "http_sendfield",
    "topic": "http",
    "info": "sends HTTP field to the peer",
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
           "name": "name",
           "type": "const char*",
           "info": "Name of the field.",
       },
       {
           "name": "value",
           "type": "const char*",
           "info": "Value of the field.",
       },
    ],

    "prologue": """
        This function sends an HTTP field, i.e. a name/value pair, to the
        peer. For example, if name is **Host** and resource is
        **www.example.org** the line sent will look like "this":

        ```
        "Host": www.example.org
        ```

        After sending the last field of HTTP request don't forget to call
        **http_done** on the socket. It will send an empty line to the
        server to let it know that the request is finished and it should
        start processing it.
    """,
    "has_handle_argument": True,
    "has_deadline": True,
    "uses_connection": True,

    "errors": ["EINVAL"],
}

new_function(http_sendfield_function)

