
tls_detach_function = {
    "name": "tls_detach",
    "topic": "tls",
    "info": "terminates TLS protocol and returns the underlying socket",
    "result": {
        "type": "int",
        "success": "underlying socket handle",
        "error": "-1",
    },
    "args": [
       {
           "name": "s",
           "type": "int",
           "info": "Handle of the TLS socket.",
       },
    ],

    "prologue": """
        This function does the terminal handshake and returns underlying
        socket to the user. The socket is closed even in the case of error.
    """,

    "has_handle_argument": True,
    "has_deadline": True,
    "uses_connection": True,

    "custom_errors": {
        "ENOTSUP": "The handle is not a TLS protocol handle.",
    },
}

new_function(tls_detach_function)

