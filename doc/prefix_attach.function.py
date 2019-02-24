
prefix_attach_function = {
    "name": "prefix_attach",
    "topic": "prefix",
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

new_topic(prefix_attach_function)

