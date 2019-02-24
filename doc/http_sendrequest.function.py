
http_sendrequest_function = {
    "name": "http_sendrequest",
    "topic": "http",
    "info": "sends initial HTTP request",
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
           "type": "const char*",
           "info": "HTTP command, such as **GET**.",
       },
       {
           "name": "resource",
           "type": "const char*",
           "info": "HTTP resource, such as **/index.html**.",
       },
    ],

    "prologue": """
        This function sends an initial HTTP request with the specified
        command and resource.  For example, if command is **GET** and
        resource is **/index.html** the line sent will look like "this":

        ```
        GET /index.html HTTP/1.1
        ```
    """,
    "has_handle_argument": True,
    "has_deadline": True,
    "uses_connection": True,

    "errors": ["EINVAL"],
}

new_function(http_sendrequest_function)

