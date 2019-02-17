
fxs.append(
    {
        "name": "ipc_fromfd",
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
                "info": "File descriptor of a connected UNIX doemain socket to wrap.",
            },
        ],

        "protocol": ipc_protocol,

        "prologue": """
            This function wraps an existing OS-level file descriptor.
        """,
        "epilogue": """
            The socket can be cleanly shut down using **ipc_close** function
            which will also close the underlying file descriptor.

            There's no way to unwrap the file descriptor.
        """,

        "allocates_handle": True,
        "mem": "ipc_storage",

        "example": """
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            connect(fd, addr, sizeof(addr));
            int s = ipc_fromfd(fd);
            bsend(s, "ABC", 3, -1);
            char buf[3];
            brecv(s, buf, sizeof(buf), -1);
            ipc_close(s);
        """
    }
)
