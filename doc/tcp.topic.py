
tcp_topic = {
    "name": "tcp",
    "title": "TCP protocol",
    "protocol": "bytestream",
    "info": """
       TCP protocol is a reliable bytestream protocol for transporting data
       over network. It is defined in RFC 793.
    """,
    "example": """
        struct ipaddr addr;
        ipaddr_local(&addr, NULL, 5555, 0);
        int ls = tcp_listen(&addr, 10);
        int s = tcp_accept(ls, NULL, -1);
        bsend(s, "ABC", 3, -1);
        char buf[3];
        brecv(s, buf, sizeof(buf), -1);
        tcp_close(s);
        tcp_close(ls);
    """,
    "storage": {"tcp" : 72},
    "opts": {
        "tcp": [
            {
                "name": "mem",
                "type": "struct tcp_storage*",
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
            {
                "name": "nodelay",
                "type": "int",
                "suffix": ":1",
                "default": "0",
                "info": "If set to true Nagle's algorithm will be switched off.",
            },
        ],
    },
}

new_topic(tcp_topic)

