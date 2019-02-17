
fxs.append(
    {
        "name": "tls_attach_client",
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
        ],
        "protocol": tls_protocol,
        "prologue": """
            This function instantiates TLS protocol on top of the underlying
            protocol. TLS protocol being asymmetric, client and server sides are
            intialized in different ways. This particular function initializes
            the client side of the connection.
        """,
        "epilogue": """
            The socket can be cleanly shut down using **tls_detach** function.
        """,
        "has_handle_argument": True,
        "has_deadline": True,
        "allocates_handle": True,
        "uses_connection": True,

        "mem": "tls_storage",

        "custom_errors": {
            "EPROTO": "Underlying socket is not a bytestream socket.",
        },
    }
)
