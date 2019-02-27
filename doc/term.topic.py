
term_topic = {
    "name": "term",
    "title": "TERM protocol",
    "protocol": "message",
    "info": """
        TERM is a protocol that implements clean terminal handshake between
        peers. When creating the protocol instance user specifies the terminal
        message to use. When closing the protocol, terminal messages are
        exchanged between peers in both directions. After the protocol shuts
        down the peers agree on their position in the message stream.
    """,
    "example": """
        s = term_attach(s, "STOP", 4);
        ...
        /* Send terminal message to the peer. */
        term_done(s, -1);
        /* Process remaining inbound messages. */
        while(1) {
            char buf[256];
            ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
            /* Check whether terminal message was received from the peer. */
            if(sz < 0 && errno == EPIPE) break;
            frobnicate(buff, sz);
        }
        s = term_detach(s);
    """,
    "storage": {"term" : 88},
    "opts": {
        "term": [
            {
                "name": "mem",
                "type": "struct term_storage*",
                "dill": True,
                "default": "NULL",
                "info": "Memory to store the object in. If NULL, the memory will be allocated automatically.",
            },
        ],
    },
}

new_topic(term_topic)

