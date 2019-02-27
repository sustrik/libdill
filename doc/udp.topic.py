
udp_topic = {
    "name": "udp",
    "title": "UDP protocol",
    "protocol": "message",
    "info": """
        UDP is an unreliable message-based protocol defined in RFC 768. The size
        of the message is limited. The protocol has no initial or terminal
        handshake. A single socket can be used to different destinations.
    """,
    "example": """
        struct ipaddr local;
        ipaddr_local(&local, NULL, 5555, 0);
        struct ipaddr remote;
        ipaddr_remote(&remote, "server.example.org", 5555, 0, -1);
        int s = udp_open(&local, &remote);
        udp_send(s1, NULL, "ABC", 3);
        char buf[2000];
        ssize_t sz = udp_recv(s, NULL, buf, sizeof(buf), -1);
        hclose(s);
    """,
    "storage": {"udp" : 72},
    "opts": {
        "udp": [
            {
                "name": "mem",
                "type": "struct udp_storage*",
                "dill": True,
                "default": "NULL",
                "info": "Memory to store the object in. If NULL, the memory will be allocated automatically.",
            },
        ],
    },
}

new_topic(udp_topic)

