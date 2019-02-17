tcp_protocol = {
    section: "TCP protocol",
    type: "bytestream",
    info: `
       TCP protocol is a reliable bytestream protocol for transporting data
       over network. It is defined in RFC 793.
    `,
    example: `
        struct ipaddr addr;
        ipaddr_local(&addr, NULL, 5555, 0);
        int ls = tcp_listen(&addr, 10);
        int s = tcp_accept(ls, NULL, -1);
        bsend(s, "ABC", 3, -1);
        char buf[3];
        brecv(s, buf, sizeof(buf), -1);
        tcp_close(s);
        tcp_close(ls);
    `
}

