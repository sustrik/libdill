
prefix_detach_function = {
    "name": "prefix_detach",
    "topic": "prefix",
    "info": "terminates PREFIX protocol and returns the underlying socket",
    "result": {
        "type": "int",
        "success": "underlying socket handle",
        "error": "-1",
    },
    "args": [
       {
           "name": "s",
           "type": "int",
           "info": "Handle of the PREFIX socket.",
       },
    ],

    "prologue": """
        This function does the terminal handshake and returns underlying
        socket to the user. The socket is closed even in the case of error.
    """,

    "has_handle_argument": True,

    "custom_errors": {
        "ENOTSUP": "The handle is not a PREFIX protocol handle.",
    },
}

new_function(prefix_detach_function)

