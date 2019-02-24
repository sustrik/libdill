
fdin_function = {
    "name": "fdin",
    "topic": "fd",
    "info": "waits on a file descriptor to become readable",

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
        socket handle) to either become readable or get into an error state.
        Either case leads to a successful return from the function. To
        distinguish the two outcomes, follow up with a read operation on
        the file descriptor.
    """,

    "has_deadline": True,

    "custom_errors": {
        "EBADF": "Not a file descriptor.",
        "EBUSY": "Another coroutine already blocked on **fdin** with " +
                 "this file descriptor.",
    },

    "example": """
        int result = fcntl(fd, F_SETFL, O_NONBLOCK);
        assert(result == 0);
        while(1) {
            result = fdin(fd, -1);
            assert(result == 0);
            char buf[1024];
            ssize_t len = recv(fd, buf, sizeof(buf), 0);
            assert(len > 0);
            process_input(buf, len);
        }
    """,
}

new_function(fdin_function)

