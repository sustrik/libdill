
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

    rc = tcp_close(s, -1);
    assert(rc == 0);

    return 0;
}

