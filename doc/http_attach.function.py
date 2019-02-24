
http_attach_function = {
    "name": "http_attach",
    "topic": "http",
    "info": "creates HTTP protocol on top of underlying socket",
    "result": {
        "type": "int",
        "success": "newly created socket handle",
        "error": "-1",
    },
    "args": [
       {
           "name": "s",
           "type": "int",
           "info": "Handle of the underlying socket. It must be a " +
                 "bytestream protocol.",
       },
       {
           "name": "opts",
           "type": "const struct http_opts*",
           "dill": True,
           "info": "Options.",
       },
    ],
    "prologue": """
        This function instantiates HTTP protocol on top of the underlying
        protocol.
    """,
    "epilogue": """
        The socket can be cleanly shut down using **http_detach** function.
    """,
    "has_handle_argument": True,
    "allocates_handle": True,
    "mem": "http_storage",

    "custom_errors": {
        "EPROTO": "Underlying socket is not a bytestream socket.",
    },
}

new_function(http_attach_function)

