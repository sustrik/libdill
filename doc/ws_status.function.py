
ws_status_function = {
    "name": "ws_status",
    "topic": "ws",
    "info": "retrieves the status after protocol termination",
    "result": {
        "type": "ssize_t",
        "success": "length of the terminal message",
        "error": "-1",
    },
    "args": [
       {
           "name": "s",
           "type": "int",
           "info": "The socket handle.",
       },
       {
           "name": "status",
           "type": "int*",
           "info": "Out parameter. Status number sent by the peer. If zero, " +
                 "there was no status.",
       },
       {
           "name": "buf",
           "type": "void*",
           "info": "Buffer to fill the terminal message into. If set to " +
                 "**NULL** the message won't be returned.",           
       },
       {
           "name": "len",
           "type": "size_t",
           "info": "Size of the buffer, in bytes.",           
       },
    ],

    "prologue": """
        When peer shuts down the protocol, receiving functions will start
        failing with **EPIPE** error. Once that happens **ws_status**
        function allows to retrieve the status and message sent by the
        peer when it closed the protocol.
    """,

    "has_handle_argument": True,
    "errors": ["EINVAL"],
    "custom_errors": {
        "EAGAIN": "The connection wasn't yet terminated by the peer.",
        "EMSGSIZE": "The message doesn't fit in the supplied buffer.",
    },
}

new_function(ws_status_function)

