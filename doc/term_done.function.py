
term_done_function = {
    "name": "term_done",
    "topic": "term",
    "info": "half-closes the connection",
    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },
    "args": [
       {
           "name": "s",
           "type": "int",
           "info": "The TERM protocol handle.",
       },
    ],

    "prologue": """
        This function sends the termination message to the peer. Afterwards,
        it is not possible to send more messages. However, it is still
        possible to receiving outstanding inbound messages.
    """,
    "has_handle_argument": True,
    "has_deadline": True,

    "custom_errors": {
        "EIPE": "The termination message was already sent.",
        "ENOTSUP": "The handle is not a TERM protocol handle.",
    },
}

new_function(term_done_function)

