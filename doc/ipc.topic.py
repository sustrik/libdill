
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
    """
}

new_topic(ipc_topic)

