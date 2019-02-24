
msendl_function = {
    "name": "msendl",
    "topic": "message",
    "info": "sends a message",
    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },
    "args": [
       {
           "name": "s",
           "type": "int",
           "info": "The socket to send the message to.",
       },
    ],

    "prologue": """
        This function sends a message to a socket. It is a blocking
        operation that unblocks only after entire message is sent. 
        There is no such thing as partial send. If a problem, including
        timeout, occurs while sending the message error is returned to the
        user and the socket cannot be used for sending from that point on.
    """,

    "has_handle_argument": True,
    "has_deadline": True,
    "has_iol": True,
    "uses_connection": True,

    "errors": ["EINVAL", "EBUSY"],
    "custom_errors": {
        "EMSGSIZE": "The message is too long.",
        "EPIPE": "Closed connection.",
    }
}

new_function(msendl_function)

