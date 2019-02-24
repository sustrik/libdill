
suffix_detach_function = {
    "name": "suffix_detach",
    "topic": "suffix",
    "info": "terminates SUFFIX protocol and returns the underlying socket",

    "result": {
        "type": "int",
        "success": "underlying socket handle",
        "error": "-1",
    },
    "args": [
       {
           "name": "s",
           "type": "int",
           "info": "Handle of the SUFFIX socket.",
       },
    ],

    "prologue": """
        This function does the terminal handshake and returns underlying
        socket to the user. The socket is closed even in the case of error.
    """,

    "has_handle_argument": True,

    "custom_errors": {
        "ENOTSUP": "The handle is not a SUFFIX protocol handle.",
    },
}

new_function(suffix_detach_function)

