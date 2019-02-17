ws_protocol = {
    section: "WebSocket protocol",
    type: "message",
    info: `
        WebSocket is a message-based protocol defined in RFC 6455. It can be
        used as a bidirectional communication channel for communication with
        a web server.
    `,
    example: `
        struct ipaddr addr;
        ipaddr_remote(&addr, "www.example.org", 80, 0, -1);
        int s = tcp_connect(&addr, -1);
        s = ws_attach_client(s, "/", "www.example.org", WS_TEXT, -1);
        ws_send(s, WS_TEXT, "Hello, world!", 13, -1);
        int flags;
        char buf[256];
        ssize_t sz = ws_recv(s, &flags, buf, sizeof(buf), -1);
        assert(flags & WS_TEXT);
        s = ws_detach(s, -1);
        tcp_close(s, -1);
    `,

    experimental: true,
}

