
bsend_function = {
    "name": "bsend",
    "topic": "bytestream",
    "info": "sends data to a socket",
    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },
    "args": [
       {
           "name": "s",
           "type": "int",
           "info": "The socket to send the data to.",
       },
       {
           "name": "buf",
           "type": "const void*",
           "info": "Buffer to send.",
       },
       {
           "name": "len",
           "type": "size_t",
           "info": "Size of the buffer, in bytes.",
       },
    ],

    "prologue": """
        This function sends data to a bytestream socket. It is a blocking
        operation that unblocks only after all the data are sent. There is
        no such thing as partial send. If a problem, including timeout,
        occurs while sending the data error is returned to the user and the
        socket cannot be used for sending from that point on.
    """,

    "has_handle_argument": True,
    "has_deadline": True,
    "uses_connection": True,

    "errors": ["EINVAL", "EBUSY"],
    "custom_errors": {
        "EPIPE": "Closed connection.",
    },

    "example": """
        int rc = bsend(s, "ABC", 3, -1);
    """,
}

new_function(bsend_function)

