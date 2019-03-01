
tls_topic = {
    "name": "tls",
    "title": "TLS protocol",
    "protocol": "bytestream",
    "info": """
        TLS is a cryptographic protocol to provide secure communication over
        the network. It is a bytestream protocol.
    """,
    "example": """
        int s = tcp_connect(&addr, -1);
        s = tls_attach_client(s, -1);
        bsend(s, "ABC", 3, -1);
        char buf[3];
        ssize_t sz = brecv(s, buf, sizeof(buf), -1);
        s = tls_detach(s, -1);
        tcp_close(s);
    """,
    "opaque": {"tls_storage" : 72},
    "opts": {
        "tls_": [
            {
                "name": "mem",
                "type": "struct tls_storage*",
                "dill": True,
                "default": "NULL",
                "info": "Memory to store the object in. If NULL, the memory will be allocated automatically.",
            },
        ],
    },
    "experimental": True,
}

new_topic(tls_topic)
