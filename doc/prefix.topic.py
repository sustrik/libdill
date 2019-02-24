
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
    """
}

new_topic(prefix_topic)

