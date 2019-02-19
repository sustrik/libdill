
fxs.append(
    {
        "name": "prefix_attach",
        "info": "creates PREFIX protocol on top of underlying socket",
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
               "name": "prefixlen",
               "type": "size_t",
               "info": "Size of the length field, in bytes."
           },
           {
               "name": "opts",
               "type": "const struct prefix_opts*",
               "dill": True,
               "info": "Options.",
           },
        ],
        "protocol": prefix_protocol,
        "prologue": """
            This function instantiates PREFIX protocol on top of the underlying
            protocol.
        """,
        "epilogue": "The socket can be cleanly shut down using **prefix_detach** " +
                  "function.",

        "has_handle_argument": True,
        "allocates_handle": True,

        "mem": "prefix_storage",

        "errors": ['EINVAL'],
        "custom_errors": {
            "EPROTO": "Underlying socket is not a bytestream socket.",
        },
    }
)
