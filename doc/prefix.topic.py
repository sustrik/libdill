
prefix_topic = {
    "name": "prefix",
    "title": "PREFIX protocol",
    "protocol": "message",
    "info": """
        PREFIX  is a message-based protocol to send binary messages prefixed by
        size. The protocol has no initial handshake. Terminal handshake is
        accomplished by each peer sending size field filled by 0xff bytes.
    """,
    "example": """
        int s = tcp_connect(&addr, -1);
        s = prefix_attach(s, 2, 0);
        msend(s, "ABC", 3, -1);
        char buf[256];
        ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
        s = prefix_detach(s, -1);
        tcp_close(s);
    """,
    "storage": {"prefix" : 56},
    "opts": {
        "prefix": [
            {
                "name": "mem",
                "type": "struct prefix_storage*",
                "dill": True,
                "default": "NULL",
                "info": "Memory to store the object in. If NULL, the memory will be allocated automatically.",
            },
            {
                "name": "little_endian",
                "type": "unsigned int",
                "suffix": ":1",
                "default": "0",
                "info": "If set to false, size prefix is going to be in network byte order (big-endian). If set to true it will be little-endian.",
            },
        ],
    },
}

new_topic(prefix_topic)

