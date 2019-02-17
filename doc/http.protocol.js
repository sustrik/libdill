http_protocol = {
    section: "HTTP protocol",
    type: "application",
    info: `
        HTTP is an application-level protocol described in RFC 7230. This
        implementation handles only the request/response exchange. Whatever
        comes after that must be handled by a different protocol.
    `,
    example: `
        int s = tcp_connect(&addr, -1);
        s = http_attach(s);
        http_sendrequest(s, "GET", "/", -1);
        http_sendfield(s, "Host", "www.example.org", -1);
        http_done(s, -1);
        char reason[256];
        http_recvstatus(s, reason, sizeof(reason), -1);
        while(1) {
            char name[256];
            char value[256];
            int rc = http_recvfield(s, name, sizeof(name), value, sizeof(value), -1);
            if(rc == -1 && errno == EPIPE) break;
        }
        s = http_detach(s, -1);
        tcp_close(s);
    `,
    experimental: true,
}

