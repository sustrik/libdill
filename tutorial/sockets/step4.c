
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "../../libdill.h"

int main(int argc, char *argv[]) {
    if(argc != 4) {
        fprintf(stderr, "Usage: wget [protocol] [server] [resource]\n");
        return 1;
    }
    int port;
    if(strcmp(argv[1], "http") == 0) port = 80;
    else if(strcmp(argv[1], "https") == 0) port = 443;
    else {
        fprintf(stderr, "Unsupported protocol.\n");
        return 1;
    }
    
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, argv[2], port, 0, -1);
    if(rc != 0) {
        perror("Cannot resolve server address");
        return 1;
    }

    int s = tcp_connect(&addr, -1);
    if(s < 0) {
        perror("Cannot connect to the remote server");
        return 1;
    }

    if(port == 443) {
       s = tls_attach_client(s, -1);
       assert(s >= 0);
    }

    s = http_attach(s);
    assert(s >= 0);

    rc = http_sendrequest(s, "GET", argv[3], -1);
    assert(rc == 0);
    rc = http_sendfield(s, "Host", argv[2], -1);
    assert(rc == 0);
    rc = http_sendfield(s, "Connection", "close", -1);
    assert(rc == 0);
    rc = http_done(s, -1);
    assert(rc == 0);

    char reason[256];
    rc = http_recvstatus(s, reason, sizeof(reason), -1);
    assert(rc >= 0);
    fprintf(stderr, "%d: %s\n", rc, reason);
    while(1) {
        char name[256];
        char value[256];
        rc = http_recvfield(s, name, sizeof(name), value, sizeof(value), -1);
        if(rc == -1 && errno == EPIPE) break;
        assert(rc == 0);
        fprintf(stderr, "%s: %s\n", name, value);
    }
    fprintf(stderr, "\n");

    s = http_detach(s, -1);
    assert(s >= 0);

    while(1) {
        unsigned char c;
        rc = brecv(s, &c, 1, -1);
        if(rc == -1 && errno == EPIPE) break;
        fprintf(stdout, "%c", c);
    }
    fprintf(stderr, "\n");

    if(port == 443) {
        s = tls_detach(s, -1);
        assert(s >= 0);
    }

    rc = tcp_close(s, -1);
    assert(rc == 0);

    return 0;
}

