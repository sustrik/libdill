
term_attach_function = {
    "name": "term_attach",
    "topic": "term",
    "info": "creates TERM protocol on top of underlying socket",
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
                 "message protocol.",
       },
       {
           "name": "buf",
           "type": "const void*",
           "info": "The terminal message.",
       },
       {
           "name": "len",
           "type": "size_t",
           "info": "Size of the terminal message, in bytes.",
       },
       {
           "name": "opts",
           "type": "const struct term_opts*",
           "dill": True,
           "info": "Options.",
       },

    ],

    "prologue": """
        This function instantiates TERM protocol on top of the underlying
        protocol.
    """,
    "epilogue": """
        The socket can be cleanly shut down using **term_detach** function.
    """,
    "has_handle_argument": True,
    "allocates_handle": True,

    "mem": "term_storage",

    "custom_errors": {
        "EPROTO": "Underlying socket is not a message socket.",
    },
}

new_function(term_attach_function)

