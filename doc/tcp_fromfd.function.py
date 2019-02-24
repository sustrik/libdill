
tcp_fromfd_function = {
    "name": "tcp_fromfd",
    "topic": "tcp",
    "info": "wraps an existing OS-level file descriptor",

    "result": {
        "type": "int",
        "success": "newly created socket handle",
        "error": "-1",
    },
    "args": [
        {
            "name": "fd",
            "type": "int",
            "info": "File descriptor of a connected TCP socket to wrap.",
        },
        {
            "name": "opts",
            "type": "const struct tcp_opts*",
            "dill": True,
            "info": "Options.",
        },
    ],

    "prologue": """
        This function wraps an existing OS-level file descriptor.
    """,
    "epilogue": """
        The socket can be cleanly shut down using **tcp_close** function
        which will also close the underlying file descriptor.

        There's no way to unwrap the file descriptor.
    """,

    "allocates_handle": True,
    "mem": "tcp_storage",

    "example": """
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        connect(fd, addr, sizeof(addr));
        int s = tcp_fromfd(fd);
        bsend(s, "ABC", 3, -1);
        char buf[3];
        brecv(s, buf, sizeof(buf), -1);
        tcp_close(s);
    """
}

new_function(tcp_fromfd_function)

