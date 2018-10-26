
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "../../libdill.h"

int main(int argc, char *argv[]) {
    if((argc < 4) || (argc >8)) {
        fprintf(stderr, "Usage: wget protocol server resource [[socks5_host socks5_port] [username password]]\n");
        return 1;
    }
    if(argc == 5) {
        fprintf(stderr, "Usage: wget protocol server resource [[socks5_host socks5_port] [username password]]\n");
        fprintf(stderr, "HINT: if you specify socks5_host, you also need socks5_port\n");
        return 1;
    }
    if(argc == 7) {
        fprintf(stderr, "Usage: wget protocol server resource [[socks5_host socks5_port] [username password]]\n");
        fprintf(stderr, "HINT: if you specify username, you also need password\n");
        return 1;
    }
    int port;
    if(strcmp(argv[1], "http") == 0) port = 80;
    else if(strcmp(argv[1], "https") == 0) port = 443;
    else {
        fprintf(stderr, "Unsupported protocol.\n");
        return 1;
    }

    int s;
    int rc;

    if (argc == 4) {
        // connect directly
        struct ipaddr addr;
        rc = ipaddr_remote(&addr, argv[2], port, 0, -1);
        if(rc != 0) {
            perror("Cannot resolve server address");
            return 1;
        }

        s = tcp_connect(&addr, -1);
        if(s < 0) {
            perror("Cannot connect to the remote server");
            return 1;
        }
    } else {
        // connect via SOCKS5 proxy
        struct ipaddr addr;
        char *host = argv[2];
        char *proxy_host = argv[4];
        int proxy_port;
        sscanf(argv[5], "%d", &proxy_port);
        if ((proxy_port < 1) || (proxy_port > 65535)) {
            fprintf(stderr, "Invalid socks5_port port number = %d\n", proxy_port);
            return 1;
        }
        rc = ipaddr_remote(&addr, proxy_host, proxy_port, 0, -1);
        if(rc != 0) {
            perror("Cannot resolve SOCKS5 proxy address");
            return 1;
        }

        s = tcp_connect(&addr, -1);
        if(s < 0) {
            perror("Cannot connect to the SOCKS5 proxy");
            return 1;
        }

        char *username = NULL;
        char *password = NULL;
        if (argc == 8) {
            username = argv[6];
            password = argv[7];
        }

        // if the address is a IPV4 or IPV6 literal, then
        // pass it as a struct ipaddr, otherwise
        struct in_addr ina4;
        struct in6_addr ina6;
        if((inet_pton(AF_INET, host, &ina4) == 1) ||
            (inet_pton(AF_INET6, host, &ina6) == 1)) {
            rc = ipaddr_remote(&addr, host, port, 0, -1);
            if(rc != 0) {
                perror("Cannot resolve SOCKS5 proxy address");
                return 1;
            }

            rc = socks5_client_connect(s, username, password, &addr, -1);
            if (rc != 0) {
                perror("Error connecting to remote host via SOCKS5 proxy");
                return 1;
            }
        } else {
            rc = socks5_client_connectbyname(s, username, password, host, port, -1);
            if (rc != 0) {
                perror("Error connecting to remote host via SOCKS5 proxy");
                return 1;
            }
        }
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
        if(rc == -1 && ((errno == EPIPE) || (ECONNRESET))) break;
        assert(rc == 0);
        fprintf(stderr, "%s: %s\n", name, value);
    }
    fprintf(stderr, "\n");

    s = http_detach(s, -1);
    assert(s >= 0);

    while(1) {
        unsigned char c;
        rc = brecv(s, &c, 1, -1);
        if(rc == -1 && ((errno == EPIPE) || (ECONNRESET))) break;
        fprintf(stdout, "%c", c);
    }
    fprintf(stderr, "\n");

    if(port == 443) {
        s = tls_detach(s, -1);
        assert(s >= 0);
    }

    rc = tcp_close(s, -1);
    if (rc != 0) {
        assert(errno == ECONNRESET);
    }

    return 0;
}
