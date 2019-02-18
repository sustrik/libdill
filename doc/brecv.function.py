
fxs.append(
    {
        "name": "brecv",
        "topic": "Bytestream sockets",
        "info": "receives data from a bytestream socket",
        "result": {
            "type": "int",
            "success": "0",
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
               "info": "Buffer to receive the data to.",
           },
           {
               "name": "len",
               "type": "size_t",
               "info": "Size of the buffer, in bytes.",
           },
        ],
        "prologue": """
            This function receives data from a bytestream socket. It is a
            blocking operation that unblocks only after the requested amount of
            data is received.  There is no such thing as partial receive.
            If a problem, including timeout, occurs while receiving the data,
            an error is returned to the user and the socket cannot be used for
            receiving from that point on.

            If **buf** is **NULL**, **len** bytes will be received but they will
            be dropped rather than returned to the user.
        """,

        "has_handle_argument": True,
        "has_deadline": True,
        "uses_connection": True,

        "errors": ["EINVAL", "EBUSY"],
        "custom_errors": {
            "EPIPE": "Closed connection.",
        },

        "example": """
            char msg[100];
            int rc = brecv(s, msg, sizeof(msg), -1);
        """,
    },
)
