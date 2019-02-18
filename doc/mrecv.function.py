
fxs.append(
    {
        "name": "mrecv",
        "topic": "Message sockets",
        "info": "receives a message",
        "result": {
            "type": "ssize_t",
            "success": "size of the received message, in bytes",
            "error": "-1",
        },
        "args": [
           {
               "name": "s",
               "type": "int",
               "info": "The socket.",
           },
           {
               "name": "buf",
               "type": "void*",
               "info": "Buffer to receive the message to.",
           },
           {
               "name": "len",
               "type": "size_t",
               "info": "Size of the buffer, in bytes.",
           },
        ],
        "prologue": """
            This function receives message from a socket. It is a blocking
            operation that unblocks only after entire message is received.
            There is no such thing as partial receive. Either entire message
            is received or no message at all.
        """,

        "has_handle_argument": True,
        "has_deadline": True,
        "uses_connection": True,

        "errors": ["EINVAL", "EBUSY", "EMSGSIZE"],
        "custom_errors": {
            "EPIPE": "Closed connection.",
        },

        "example": """
            char msg[100];
            ssize_t msgsz = mrecv(s, msg, sizeof(msg), -1);
        """,
    }
)
