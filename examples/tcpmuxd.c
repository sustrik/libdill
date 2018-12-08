/*

  Copyright (c) 2018 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libdill.h"
#include "../list.h"

struct service {
    struct dill_list item;
    char name[256];
    int s;
};

struct dill_list services;

coroutine void tcp_handler(int s) {
    int64_t deadline = now() + 1000000;
    struct suffix_storage mem;
    struct suffix_opts opts = suffix_defaults;
    opts.mem = &mem;
    s = suffix_attach(s, "\r\n", 2, &opts);
    assert(s >= 0);
    /* Read the requested service name. */
    char name[256];
    ssize_t sz = mrecv(s, name, sizeof(name), deadline);
    assert(sz >= 0);
    assert(sz > 0 && sz < 256);
    name[sz] = 0;
    int i;
    for(i = 0; i != sz; ++i) {
        assert(name[i] >= 32 && name[i] <= 127);
        name[i] = (char)tolower(name[i]);
    }
    /* Find the registered service. */
    struct dill_list *it;
    struct service *svc;
    for(it = dill_list_next(&services); it != &services;
          it = dill_list_next(it)) {
         svc = dill_cont(it, struct service, item);
         if(strcmp(svc->name, name) == 0) break;
    }
    int success = it != &services;
    /* Reply to the TCP peer. */
    const char *reply = success ? "+" : "-Service not found";
    int rc = msend(s, reply, strlen(reply), deadline);
    assert(rc == 0);
    if(!success) {
        fprintf(stderr, "%s: not found\n", name);
        rc = hclose(s);
        assert(rc == 0);
        return;
    }
    /* Send the raw fd to the process implementing the service. */
    s = suffix_detach(s);
    assert(s >= 0);
    s = tcp_detach(s);
    assert(s >= 0);
    rc = ipc_sendfd(svc->s, s, deadline);
    assert(rc == 0);
    fprintf(stderr, "%s: connection created\n", svc->name);
}

coroutine void ipc_handler(int s) {
    /* Get the service registration details. */
    int64_t deadline = now() + 1000000;
    struct service *svc = malloc(sizeof(struct service));
    assert(svc);
    memset(svc, 0, sizeof(struct service));
    uint8_t val;
    int rc = brecv(s, &val, 1, deadline);
    assert(rc == 0);
    rc = brecv(s, svc->name, val, deadline);
    assert(rc == 0);
    svc->name[val] = 0;
    svc->s = s;
    /* Check whether the service already exists. */
    struct dill_list *it;
    for(it = dill_list_next(&services); it != &services;
          it = dill_list_next(it)) {
        struct service *svc_it = dill_cont(it, struct service, item);
        if(strcmp(svc->name, svc_it->name) == 0) {
            /* Check whether the service provider is still connected. */
            rc = brecv(svc_it->s, &val, 1, 0);
            if(rc < 0 && (errno == ECONNRESET || errno == EPIPE)) {
                fprintf(stderr, "%s: unregistered\n", svc_it->name);
                rc = hclose(svc_it->s);
                assert(rc == 0);
                dill_list_erase(it);
                free(svc_it);
                break;
            }
            assert(rc == 0);
            val = 0;
            rc = bsend(s, &val, 1, deadline);
            assert(rc == 0);
            rc = hclose(s);
            assert(rc == 0);
            return;
        }
    }
    /* Store the registration record. */
    dill_list_insert(&svc->item, &services);
    fprintf(stderr, "%s: registered\n", svc->name);
    /* Report success to the application. */
    val = 1;
    rc = bsend(s, &val, 1, deadline);
    assert(rc == 0);
}

coroutine void tcp_listener(int lst) {
    struct tcp_opts opts = tcp_defaults;
    opts.rx_buffering = 0;
    while(1) {
        int s = tcp_accept(lst, &opts, NULL, -1);
        if(s < 0) {
            perror("Can't accept incoming TCP connection");
            exit(1);
        }
        int rc = go(tcp_handler(s));
        if(s < 0) {
            perror("Can't launch TCP handler coroutine");
            exit(1);
        }
    }
}

coroutine void ipc_listener(int lst) {
    struct ipc_opts opts = ipc_defaults;
    opts.rx_buffering = 0;
    while(1) {
        int s = ipc_accept(lst, &opts, -1);
        if(s < 0) {
            perror("Can't accept incoming IPC connection");
            exit(1);
        }
        int rc = go(ipc_handler(s));
        if(s < 0) {
            perror("Can't launch IPC handler coroutine");
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    /* Parse command-line arguments. */
    if(argc > 3) {
        fprintf(stderr, "usage: tcpmuxd [<port> [<filename>]]\n"
            "default port: 10001\n"
            "default filename: /tmp/tcpmux\n");
        return 1;
    }
    int port = argc > 1 ? atoi(argv[1]) : 10001;
    const char *filename = argc > 2 ? argv[2] : "/tmp/tcpmux";

    unlink(filename);
    dill_list_init(&services);

    /* Start listening for incoming TCP connections. */
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, port, 0);
    if(rc < 0) {
        perror("Can't resolve local IP address");
        return 1;
    }
    int tcps = tcp_listen(&addr, NULL);
    if(tcps < 0) {
        perror("Can't listen on the TCP port");
        return 1;
    }
    rc = go(tcp_listener(tcps));
    if(rc < 0) {
        perror("Failed to start TCP listener coroutine");
        return 1;
    }

    /* Start listening for incoming IPC connections. */
    int ipcs = ipc_listen(filename, NULL);
    if(ipcs < 0) {
        perror("Can't listen on the IPC file");
        return 1;
    }
    ipc_listener(ipcs);

    return 0;
}

