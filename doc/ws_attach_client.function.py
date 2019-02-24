
ws_attach_client_function = {
    "name": "ws_attach_client",
    "topic": "ws",
    "info": "creates WebSocket protocol on the top of underlying socket",
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
           "name": "resource",
           "type": "const char*",
           "info": "HTTP resource to use.",
       },
       {
           "name": "host",
           "type": "const char*",
           "info": "Virtual HTTP host to use.",
       },
       {
           "name": "opts",
           "type": "const struct ws_opts*",
           "dill": True,
           "info": "Options.",
       },
    ],

    "prologue": """
        This function instantiates WebSocket protocol on top of the
        underlying bytestream protocol. WebSocket protocol being asymmetric,
        client and server sides are intialized in different ways. This
        particular function initializes the client side of the connection.
    """ + ws_flags,
    "epilogue": """
        The socket can be cleanly shut down using **ws_detach** function.
    """,
    "has_handle_argument": True,
    "has_deadline": True,
    "allocates_handle": True,
    "uses_connection": True,

    "mem": "ws_storage",

    "errors": ["EINVAL"],

    "custom_errors": {
        "EPROTO": "Underlying socket is not a bytestream socket.",
    },
}

new_function(ws_attach_client_function)

