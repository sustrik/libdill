
fdout_function = {
    "name": "fdout",
    "topic": "fd",
    "info": "wait on file descriptor to become writable",

    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },
  
    "args": [
        {
            "name": "fd",
            "type": "int",
            "info": "file descriptor (OS-level one, not a libdill handle)"
        },
    ],

    "prologue": """
        Waits on a file descriptor (true OS file descriptor, not libdill
        socket handle) to either become writeable or get into an error
        state. Either case leads to a successful return from the function.
        To distinguish the two outcomes, follow up with a write operation on
        the file descriptor.
    """,

    "has_deadline": True,

    "custom_errors": {
        "EBADF": "Not a file descriptor.",
        "EBUSY": "Another coroutine already blocked on **fdout** with " +
                 "this file descriptor.",
    },

    "example": """
        int result = fcntl(fd, F_SETFL, O_NONBLOCK);
        assert(result == 0);
        while(len) {
            result = fdout(fd, -1);
            assert(result == 0);
            ssize_t sent = send(fd, buf, len, 0);
            assert(len > 0);
            buf += sent;
            len -= sent;
        }
    """,
}

new_function(fdout_function)

