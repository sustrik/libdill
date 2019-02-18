
fxs.append(
    {
        "name": "tls_attach_server",
        "info": "creates TLS protocol on top of underlying socket",
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
               "name": "cert",
               "type": "const char*",
               "info": "Filename of the file contianing the certificate.",
           },
           {
               "name": "pkey",
               "type": "const char*",
               "info": "Filename of the file contianing the private key.",
           },
        ],
        "protocol": tls_protocol,
        "prologue": """
            This function instantiates TLS protocol on top of the underlying
            protocol. TLS protocol being asymmetric, client and server sides are
            intialized in different ways. This particular function initializes
            the server side of the connection.
        """,
        "epilogue": """
            The socket can be cleanly shut down using **tls_detach** function.
        """,
        "has_handle_argument": True,
        "has_deadline": True,
        "allocates_handle": True,
        "uses_connection": True,

        "mem": "tls_storage",

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
)
