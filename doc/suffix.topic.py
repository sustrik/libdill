
suffix_topic = {
    "name": "suffix",
    "title": "SUFFIX protocol",
    "protocol": "message",
    "info": """
        SUFFIX is a message-based protocol that delimits messages by specific
        byte sequences. For example, many protocols are line-based, with
        individual messages separated by CR+LF sequence.
    """,
    "example": """
        int s = tcp_connect(&addr, -1);
        s = suffix_attach(s, "\r\n", 2);
        msend(s, "ABC", 3, -1);
        char buf[256];
        ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
        s = suffix_detach(s, -1);
        tcp_close(s);
    """,
    "opaque": {"suffix_storage" : 128},
    "opts": {
        "suffix_": [
            {
                "name": "mem",
                "type": "struct suffix_storage*",
                "dill": True,
                "default": "NULL",
                "info": "Memory to store the object in. If NULL, the memory will be allocated automatically.",
            },
        ],
    },
}

new_topic(suffix_topic)

