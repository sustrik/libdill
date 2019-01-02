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

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "list.h"
#include "utils.h"

const struct dill_tcpmux_opts dill_tcpmux_defaults = {
    NULL,           /* mem */
    "/tmp/tcpmux",  /* addr */
    10001,          /* port - RFC1078 specifies port 1, however, superpowers
                       are needed to listen on port 1 so use port 10001. */
};

dill_unique_id(dill_tcpmux_type);

static void *dill_tcpmux_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_tcpmux_hclose(struct dill_hvfs *hvfs);

struct dill_tcpmux_sock {
    struct dill_hvfs hvfs;
    int s;
    struct dill_ipc_storage ipcmem;
    char addr[256];
    char service[256];
    unsigned int mem : 1;
};

DILL_CHECK_STORAGE(dill_tcpmux_sock, dill_tcpmux_storage)

static void *dill_tcpmux_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_tcpmux_sock *self = (struct dill_tcpmux_sock*)hvfs;
    if(type == dill_tcpmux_type) return self;
    errno = ENOTSUP;
    return NULL;
}

static void dill_tcpmux_hclose(struct dill_hvfs *hvfs) {
    struct dill_tcpmux_sock *self = (struct dill_tcpmux_sock*)hvfs;
    if(dill_fast(self->s >= 0)) {
        int rc = dill_hclose(self->s);
        dill_assert(rc == 0);
    }
    if(!self->mem) free(self);
}

/* Establish connection to tcpmuxd. This function is called when the listening
   socket is created as well as after the connection to tcpmuxd breaks. */
static int dill_tcpmux_register(struct dill_tcpmux_sock *self,
      int64_t deadline) {
    struct dill_ipc_opts ipcopts = dill_ipc_defaults;
    ipcopts.rx_buffering = 0;
    ipcopts.mem = &self->ipcmem;
    self->s = dill_ipc_connect(self->addr, &ipcopts, deadline);
    if(dill_slow(self->s < 0)) return -1;
    uint8_t val = (uint8_t)strlen(self->service);
    int rc = dill_bsend(self->s, &val, 1, deadline);
    if(dill_slow(rc < 0)) return -1;
    rc = dill_bsend(self->s, self->service, strlen(self->service), deadline);
    if(dill_slow(rc < 0)) return -1;
    rc = dill_brecv(self->s, &val, 1, deadline);
    if(dill_slow(rc < 0)) return -1;
    if(dill_slow(val == 0)) {errno = EPROTO; return -1;} /* TODO: error code? */
    return 0;
}

int dill_tcpmux_listen(const char *service,
      const struct dill_tcpmux_opts *opts, int64_t deadline) {
    int err;
    if(dill_slow(!service)) {err = EINVAL; goto error1;}
    if(!opts) opts = &dill_tcpmux_defaults;
    size_t servicelen = strlen(service);
    if(dill_slow(servicelen > 255)) {err = EINVAL; goto error1;}
    size_t addrlen = strlen(opts->addr);
    if(dill_slow(addrlen > 255)) {err = EINVAL; goto error1;}
    /* Create the object. */
    struct dill_tcpmux_sock *self = (struct dill_tcpmux_sock*)opts->mem;
    if(!self) {
        self = malloc(sizeof(struct dill_tcpmux_sock));
        if(dill_slow(!self)) {err = ENOMEM; goto error1;}
    }
    self->hvfs.query = dill_tcpmux_hquery;
    self->hvfs.close = dill_tcpmux_hclose; 
    memcpy(self->addr, opts->addr, addrlen + 1);
    memcpy(self->service, service, servicelen + 1);
    self->mem = !!opts->mem;
    int rc = dill_tcpmux_register(self, deadline);
    if(dill_slow(rc != 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = dill_hmake(&self->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error3;}
    return h;
error3:
    rc = dill_hclose(self->s);
    dill_assert(rc == 0);
error2:
    if(!self->mem) free(self);
error1:
    errno = err;
    return -1;
}

int dill_tcpmux_accept(int s, const struct dill_tcp_opts *opts,
      struct dill_ipaddr *addr, int64_t deadline) {
    struct dill_tcpmux_sock *self = dill_hquery(s, dill_tcpmux_type);
    if(dill_slow(!self)) return -1;
    if(!opts) opts = &dill_tcp_defaults;
    while(1) {
        int as = dill_ipc_recvfd(self->s, deadline);
        if(dill_fast(as >= 0)) {
            as = dill_tcp_fromfd(as, opts);
            dill_assert(as >= 0);
            return as;
        }
        if(dill_slow(errno != ECONNRESET && errno != EPIPE)) return -1;
        int rc = dill_hclose(self->s);
        dill_assert(rc == 0);
        /* Connection to tcpmuxd broken. Try reestablishing it once a second. */
        while(1) {
            rc = dill_tcpmux_register(self, deadline);
            if(dill_fast(rc == 0)) break;
            if(dill_slow(errno != ECONNREFUSED)) return -1;
            int64_t onesec = dill_now () + 1000;
            rc = dill_msleep(
                (deadline < 0 || onesec < deadline) ? onesec : deadline);
            if(dill_slow(rc < 0)) return -1;
        }
    }
}

int dill_tcpmux_switch(int s, const char *service, int64_t deadline) {
    int err;
    if(dill_slow(s < 0 || !service)) {err = EINVAL; goto error1;}
    /* Sanity-check the service name. */
    size_t servicelen = strlen(service);
    if(dill_slow(servicelen > 255)) {errno = EINVAL; goto error1;}
    /* Layer CRLF protocol on top of the underlying socket. */
    struct dill_suffix_storage sfxmem;
    struct dill_suffix_opts sfxopts = dill_suffix_defaults;
    sfxopts.mem = &sfxmem;
    int rc = dill_suffix_attach(s, "\r\n", 2, &sfxopts);
    dill_assert(rc == 0);
    /* Send request to tcpmuxd asking it to pass the connection to the server
       implementing specified service. */
    rc = dill_msend(s, service, servicelen, deadline);
    dill_assert(rc == 0);
    /* Process reply from tcpmuxd. */
    char buf[1]; 
    memset(buf, 0, sizeof(buf));
    ssize_t sz = dill_mrecv(s, buf, 1, deadline);
    if(dill_slow(sz < 0)) {err = errno; goto error2;}
    if(dill_slow(buf[0] != '+' && buf[0] != '-')) {err = EPROTO; goto error2;}
    if(dill_slow(buf[0] == '-')) {err = ECONNREFUSED; goto error2;}
    /* Terminate the CRLF layer. */
    rc = dill_suffix_detach(s);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    return 0;
error2:
    rc = dill_hclose(s);
    dill_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

/******************************************************************************/
/* The TCPMUX daemon.                                                         */
/******************************************************************************/

struct dill_tcpmuxd_service {
    struct dill_list item;
    char name[256];
    int s;
};

static dill_coroutine void dill_tcpmuxd_tcp_handler(int s,
      struct dill_list *services) {
    int err;
    int64_t deadline = dill_now() + 10000;
    struct dill_suffix_storage mem;
    struct dill_suffix_opts opts = dill_suffix_defaults;
    opts.mem = &mem;
    int rc = dill_suffix_attach(s, "\r\n", 2, &opts);
    if(dill_slow(rc != 0)) {err = errno; goto error1;}
    /* Read the requested service name. */
    char name[256];
    ssize_t sz = dill_mrecv(s, name, sizeof(name), deadline);
    if(dill_slow(sz < 0)) {err = errno; goto error1;}
    if(dill_slow(sz > 255)) {err = ENAMETOOLONG; goto error1;}
    name[sz] = 0;
    /* Service names are case-insensitive. */
    int i;
    for(i = 0; i != sz; ++i) name[i] = (char)tolower(name[i]);
    /* Find the registered service. */
    struct dill_list *it;
    struct dill_tcpmuxd_service *svc;
    for(it = dill_list_next(services); it != services;
          it = dill_list_next(it)) {
         svc = dill_cont(it, struct dill_tcpmuxd_service, item);
         if(strcmp(svc->name, name) == 0) break;
    }
    int success = (it != services);
    /* Reply to the TCP peer. */
    const char *reply = success ? "+" : "-Service not found";
    rc = dill_msend(s, reply, strlen(reply), deadline);
    if(dill_slow(rc != 0)) {err = errno; goto error1;}
    if(!success) {
        fprintf(stderr, "%s: not found\n", name);
        err = EADDRNOTAVAIL;
        goto error1;
    }
    /* Send the raw fd to the process implementing the service. */
    rc = dill_suffix_detach(s);
    if(dill_slow(rc != 0)) {err = errno; goto error1;}
    int fd = dill_tcp_tofd(s);
    if(dill_slow(rc != 0)) {err = errno; goto error1;}
    s = -1; // ???
    rc = dill_ipc_sendfd(svc->s, fd, deadline);
    if(dill_slow(rc != 0)) {err = errno; goto error1;}
    fprintf(stderr, "%s: connection created\n", svc->name);
    return;
error1:
    if(s >= 0) {
        rc = dill_hclose(s);
        dill_assert(rc == 0);
    }
}

static dill_coroutine void dill_tcpmuxd_tcp_listener(int tcps,
      struct dill_list *services) {
    int err;
    struct dill_tcp_opts opts = dill_tcp_defaults;
    opts.rx_buffering = 0;
    int tcpb = dill_bundle(NULL);
    if(dill_slow(tcpb < 0)) {err = errno; goto error1;}
    while(1) {
        int s = dill_tcp_accept(tcps, &opts, NULL, -1);
        if(dill_slow(s < 0)) {err = errno; goto error2;}
        int rc = dill_bundle_go(tcpb, dill_tcpmuxd_tcp_handler(s, services));
        if(dill_slow(s < 0)) {err = errno; goto error2;}
    }
error2:;
    int rc = dill_hclose(tcpb);
    dill_assert(rc == 0);
error1:;
}

static dill_coroutine void dill_tcpmuxd_ipc_handler(int s,
      struct dill_list *services) {
    int err;
    /* Get the service registration details. */
    int64_t deadline = dill_now() + 10000;
    struct dill_tcpmuxd_service *svc =
        malloc(sizeof(struct dill_tcpmuxd_service));
    if(dill_slow(!svc)) {err = ENOMEM; goto error1;}
    memset(svc, 0, sizeof(struct dill_tcpmuxd_service));
    uint8_t svclen;
    int rc = dill_brecv(s, &svclen, 1, deadline);
    if(dill_slow(rc != 0)) {err = errno; goto error2;}
    rc = dill_brecv(s, svc->name, svclen, deadline);
    if(dill_slow(rc != 0)) {err = errno; goto error2;}
    svc->name[svclen] = 0;
    svc->s = s;
    /* Service names are case-insensitive. */
    int i;
    for(i = 0; i != svclen; ++i) svc->name[i] = (char)tolower(svc->name[i]);
    /* Check whether the service already exists. */
    struct dill_list *it;
    for(it = dill_list_next(services); it != services;
          it = dill_list_next(it)) {
        struct dill_tcpmuxd_service *svc_it = dill_cont(it,
            struct dill_tcpmuxd_service, item);
        if(strcmp(svc->name, svc_it->name) == 0) {
            /* Check whether the old service provider is still connected. */
            rc = dill_brecv(svc_it->s, NULL, 1, 0);
            if(rc < 0 && (errno == ECONNRESET || errno == EPIPE)) {
                fprintf(stderr, "%s: unregistered\n", svc_it->name);
                rc = dill_hclose(svc_it->s);
                if(dill_slow(rc != 0)) {err = errno; goto error2;}
                dill_list_erase(it);
                free(svc_it);
                break;
            }
            if(dill_slow(rc != 0)) {err = errno; goto error2;}
            int result = 0;
            rc = dill_bsend(s, &result, 1, deadline);
            if(dill_slow(rc != 0)) {err = errno; goto error2;}
            err = EADDRINUSE;
            goto error2;
        }
    }
    /* Store the registration record. */
    dill_list_insert(&svc->item, services);
    fprintf(stderr, "%s: registered\n", svc->name);
    /* Report success to the application. */
    int result = 1;
    rc = dill_bsend(s, &result, 1, deadline);
    if(dill_slow(rc != 0)) {err = errno; goto error3;}
    return;
error3:
    dill_list_erase(&svc->item);
error2:
    free(svc);
error1:
    rc = dill_hclose(s);
    dill_assert(rc == 0);
}

int tcpmux_daemon(const struct dill_tcpmux_opts *opts) {
    int err;
    if(!opts) opts = &dill_tcpmux_defaults;
    /* Get rid of the file from failed previous run. This is a known POSIX
       race condition. If there's another TCPMUX daemon running we delete the
       file nonetheless. */
    unlink(opts->addr);
    /* List of all registered services. */
    struct dill_list services;
    dill_list_init(&services);
    /* Start listening for incoming TCP connections. */
    struct dill_ipaddr addr;
    int rc = dill_ipaddr_local(&addr, NULL, opts->port, 0);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    int tcps = dill_tcp_listen(&addr, NULL);
    if(dill_slow(tcps < 0)) {err = errno; goto error1;}
    int tcph = dill_go(dill_tcpmuxd_tcp_listener(tcps, &services));
    if(dill_slow(tcph < 0)) {err = errno; goto error2;}    
    /* Start listening for incoming IPC connections. */
    int ipcs = dill_ipc_listen(opts->addr, NULL);
    if(dill_slow(ipcs < 0)) {err = errno; goto error3;}
    struct dill_ipc_opts iopts = dill_ipc_defaults;
    iopts.rx_buffering = 0;
    int ipcb = dill_bundle(NULL);
    if(dill_slow(ipcb < 0)) {err = errno; goto error4;}
    int s;
    while(1) {
        s = dill_ipc_accept(ipcs, &iopts, -1);
        if(dill_slow(s < 0)) {err = errno; goto error5;}
        int rc = dill_bundle_go(ipcb, dill_tcpmuxd_ipc_handler(s, &services));
        if(dill_slow(s < 0)) {err = errno; goto error6;}
    }
error6:
    rc = dill_hclose(s);
    dill_assert(rc == 0);
error5:
    rc = dill_hclose(ipcb);
    dill_assert(rc == 0);
error4:
    rc = dill_hclose(ipcs);
    dill_assert(rc == 0);
error3:
    rc = dill_hclose(tcph);
    dill_assert(rc == 0);
error2:
    rc = dill_hclose(tcps);
    dill_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

