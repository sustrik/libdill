
ipc_topic = {
    "name": "ipc",
    "title": "IPC protocol",
    "protocol": "bytestream",
    "info": """
       IPC  protocol is a bytestream protocol for transporting data among
       processes on the same machine.  It is an equivalent to POSIX
       **AF_LOCAL** sockets.
    """,
    "example": """
        int ls = ipc_listen("/tmp/test.ipc", 10);
        int s = ipc_accept(ls, -1);
        bsend(s, "ABC", 3, -1);
        char buf[3];
        brecv(s, buf, sizeof(buf), -1);
        ipc_close(s);
        ipc_close(ls);
    """,
    "opaque": {"ipc_storage" : 72},
    "opts": {
        "ipc_": [
            {
                "name": "mem",
                "type": "struct ipc_storage*",
                "dill": True,
                "default": "NULL",
                "info": "Memory to store the object in. If NULL, the memory will be allocated automatically.",
            },
            {
                "name": "backlog",
                "type": "int",
                "default": "64",
                "info": "Size of connection backlog. The value is relevant only for listening sockets.",
            },
            {
                "name": "rx_buffering",
                "type": "unsigned int",
                "suffix": ":1",
                "default": "1",
                "info": "If set to false the connection will do no read-aheads.",
            },
        ],
    },
}

new_topic(ipc_topic)

