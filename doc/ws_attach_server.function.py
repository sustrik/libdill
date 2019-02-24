
ws_attach_server_function = {
    "name": "ws_attach_server",
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
           "name": "opts",
           "type": "const struct ws_opts*",
           "dill": True,
           "info": "Options.",
       },

       {
           "name": "resource",
           "type": "char*",
           "info": "Out parameter. HTTP resource specified by the client.",
       },
       {
           "name": "resourcelen",
           "type": "size_t",
           "info": "Size of the **resource** buffer.",
       },
       {
           "name": "host",
           "type": "char*",
           "info": "Out parameter. Virtual host specified by the client.",
       },
       {
           "name": "hostlen",
           "type": "size_t",
           "info": "Size of the **host** buffer.",
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

    "example": """
        int s = tcp_accept(listener, NULL, -1);
        s = tls_attach_server(s, -1);
        bsend(s, "ABC", 3, -1);
        char buf[3];
        ssize_t sz = brecv(s, buf, sizeof(buf), -1);
        s = tls_detach(s, -1);
        tcp_close(s);
    """,
}

new_function(ws_attach_server_function)

