
ws_recv_function = {
    "name": "ws_recv",
    "topic": "ws",
    "info": "receives a WebSocket message",
    "result": {
        "type": "ssize_t",
        "success": "size of the received message, in bytes",
        "error": "-1",
    },
    "args": [
       {
           "name": "s",
           "type": "int",
           "info": "The socket handle.",
       },
       {
           "name": "flags",
           "type": "int*",
           "info": "Out parameter. Possible values are either **WS_BINARY**" +
                 "or **WS_TEXT**.",
       },
       {
           "name": "buf",
           "type": "void*",
           "info": "Buffer to receive the data to.",
       },
       {
           "name": "len",
           "type": "size_t",
           "info": "Size of the buffer, in bytes.",
       },
    ],

    "prologue": """
        This function receives one WebSocket message. It is very much the
        same as **mrecv** except that it returns the type of the message
        (either text or binary).
    """,

    "has_handle_argument": True,
    "uses_connection": True,
    "has_deadline": True,

    "errors": ["EINVAL", "EBUSY", "EMSGSIZE"],
}

new_function(ws_recv_function)

