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

#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "libdillimpl.h"
#include "fd.h"
#include "utils.h"

static void tls_init(void);
static int tls_makeconn(int fd, SSL *ssl, void *mem);
static int tls_followup(int fd, SSL *ssl, int rc, int64_t deadline);

dill_unique_id(tls_type);
dill_unique_id(tls_listener_type);

/******************************************************************************/
/*  TLS connection socket                                                     */
/******************************************************************************/

static void *tls_hquery(struct hvfs *hvfs, const void *type);
static void tls_hclose(struct hvfs *hvfs);
static int tls_hdone(struct hvfs *hvfs, int64_t deadline);
static int tls_bsendl(struct bsock_vfs *bvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static int tls_brecvl(struct bsock_vfs *bvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

struct tls_conn {
    struct hvfs hvfs;
    struct bsock_vfs bvfs;
    int fd;
    SSL *ssl;
    unsigned int indone : 1;
    unsigned int outdone: 1;
    unsigned int inerr : 1;
    unsigned int outerr : 1;
    unsigned int mem : 1;
};

DILL_CT_ASSERT(TLS_SIZE >= sizeof(struct tls_conn));

static void *tls_hquery(struct hvfs *hvfs, const void *type) {
    struct tls_conn *self = (struct tls_conn*)hvfs;
    if(type == bsock_type) return &self->bvfs;
    if(type == tls_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int tls_connect_mem(const struct ipaddr *addr, void *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    /* If needed, initialize OpenSSL library. */
    tls_init();
    /* Open a socket. */
    int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fd_unblock(s);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Connect to the remote endpoint. */
    rc = fd_connect(s, ipaddr_sockaddr(addr), ipaddr_len(addr), deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Create OpenSSL connection context. */
    const SSL_METHOD *method = SSLv23_method();
    if(dill_slow(!method)) {errno = EFAULT; goto error2;}
    SSL_CTX *ctx = SSL_CTX_new(method);
    if(dill_slow(!ctx)) {errno = EFAULT; goto error3;}
    /* Create OpenSSL connection object. */
    SSL *ssl = SSL_new(ctx);
    if(dill_slow(!ssl)) {err = EFAULT; goto error3;}
    rc = SSL_set_fd(ssl, s);
    if(dill_slow(rc != 1)) {err = EFAULT; goto error3;}
    /* Do the TLS protocol initialization. */
    while(1) {
        errno = 0;
        ERR_clear_error();
        rc = SSL_connect(ssl);
        if(tls_followup(s, ssl, rc, deadline)) break;
    }
    if(dill_slow(errno != 0)) {err = errno; goto error3;}
    /* Create the handle. */
    int h = tls_makeconn(s, ssl, mem);
    if(dill_slow(h < 0)) {err = errno; goto error3;}
    SSL_CTX_free(ctx);  
    return h;
error3:
    SSL_CTX_free(ctx);
error2:
    fd_close(s);
error1:
    errno = err;
    return -1;
}

int tls_connect(const struct ipaddr *addr, int64_t deadline) {
    int err;
    struct tls_conn *obj = malloc(TLS_SIZE);
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int s = tls_connect_mem(addr, obj, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return s;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

static int tls_bsendl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct tls_conn *self = dill_cont(bvfs, struct tls_conn, bvfs);
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    struct iolist *it = first;
    while(1) {
        uint8_t *base = it->iol_base;
        size_t len = it->iol_len;
        while(1) {
            errno = 0;
            ERR_clear_error();
            int rc = SSL_write(self->ssl, base, len);
            if(tls_followup(self->fd, self->ssl, rc, deadline)) break;
        }
        if(dill_slow(errno != 0)) {self->outerr = 1; return -1;}
        if(it == last) break;
        it = it->iol_next;
    }
    return 0;
}

static int tls_brecvl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct tls_conn *self = dill_cont(bvfs, struct tls_conn, bvfs);
    if(dill_slow(self->indone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->inerr)) {errno = ECONNRESET; return -1;}
    struct iolist *it = first;
    while(1) {
        uint8_t *base = it->iol_base;
        size_t len = it->iol_len;
        while(1) {
            errno = 0;
            ERR_clear_error();
            int rc = SSL_read(self->ssl, base, len);
            if(tls_followup(self->fd, self->ssl, rc, deadline)) break;
        }
        if(dill_slow(errno != 0)) {
            if(errno == EPIPE) self->indone = 1;
            else self->inerr = 1;
            return -1;
        }
        if(it == last) break;
        it = it->iol_next;
    }
    return 0;
}

int tls_close(int s, int64_t deadline) {
    int err;
    /* Listener socket needs no special treatment. */
    if(hquery(s, tls_listener_type)) {
        return hclose(s);
    }
    struct tls_conn *self = hquery(s, tls_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->inerr || self->outerr)) {err = ECONNRESET; goto error;}
    /* Start terminal TLS handshake. */
    if(!self->outdone) {
        int rc = tls_hdone(&self->hvfs, deadline);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    /* Wait for the handshake acknowledgement from the peer. */
    if(!self->indone) {
        while(1) {
            errno = 0;
            ERR_clear_error();
            int rc = SSL_shutdown(self->ssl);
            if(rc == 1) break;
            if(tls_followup(self->fd, self->ssl, rc, deadline)) {
                err = errno; goto error;}
        }
    }
    /* No need to do TCP handshake here. TLS handshake is sufficient to make
       sure that the peer have received all the data. */
    tls_hclose(&self->hvfs);
    return 0;
error:
    tls_hclose(&self->hvfs);
    errno = err;
    return -1;
}

static int tls_hdone(struct hvfs *hvfs, int64_t deadline) {
    struct tls_conn *self = (struct tls_conn*)hvfs;
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    while(1) {
        errno = 0;
        ERR_clear_error();
        int rc = SSL_shutdown(self->ssl);
        if(rc == 0) {self->outdone = 1; return 0;}
        if(rc == 1) {self->outdone = 1; self->indone = 1; return 0;}
        if(tls_followup(self->fd, self->ssl, rc, deadline)) break;
    }
    dill_assert(errno != 0);
    self->outerr = 1;
    return -1;
}

static void tls_hclose(struct hvfs *hvfs) {
    struct tls_conn *self = (struct tls_conn*)hvfs;
    fd_close(self->fd);
    SSL_free(self->ssl);
    if(!self->mem) free(self);
}

/******************************************************************************/
/*  TLS listener socket                                                       */
/******************************************************************************/

static void *tls_listener_hquery(struct hvfs *hvfs, const void *type);
static void tls_listener_hclose(struct hvfs *hvfs);

struct tls_listener {
    struct hvfs hvfs;
    int fd;
    SSL_CTX *ctx;
    struct ipaddr addr;
    unsigned int mem : 1;
};

DILL_CT_ASSERT(TLS_LISTENER_SIZE >= sizeof(struct tls_listener));

static void *tls_listener_hquery(struct hvfs *hvfs, const void *type) {
    struct tls_listener *self = (struct tls_listener*)hvfs;
    if(type == tls_listener_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int tls_listen_mem(struct ipaddr *addr, const char *cert, const char *pkey,
      int backlog, void *mem) {
    int err;
    if(dill_slow(!mem || !cert || !pkey)) {err = EINVAL; goto error1;}
    /* If needed, initialize OpenSSL library. */
    tls_init();
    /* Create the SSL context object. */
    const SSL_METHOD *method = SSLv23_server_method();
    if(dill_slow(!method)) {errno = EFAULT; goto error1;}
    SSL_CTX *ctx = SSL_CTX_new(method);
    if(dill_slow(!ctx)) {errno = EFAULT; goto error1;}
    int rc = SSL_CTX_set_ecdh_auto(ctx, 1);
    if(dill_slow(rc != 1)) {errno = EFAULT; goto error2;}
    rc = SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM);
    if(dill_slow(rc <= 0)) {errno = EFAULT; goto error2;}
    rc = SSL_CTX_use_PrivateKey_file(ctx, pkey, SSL_FILETYPE_PEM);
    if(dill_slow(rc <= 0 )) {errno = EFAULT; goto error2;}
    /* Open the listening socket. */
    int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
    if(dill_slow(s < 0)) {err = errno; goto error2;}
    /* Set it to non-blocking mode. */
    rc = fd_unblock(s);
    if(dill_slow(rc < 0)) {err = errno; goto error3;}
    /* Start listening for incoming connections. */
    rc = bind(s, ipaddr_sockaddr(addr), ipaddr_len(addr));
    if(dill_slow(rc < 0)) {err = errno; goto error3;}
    rc = listen(s, backlog);
    if(dill_slow(rc < 0)) {err = errno; goto error3;}
    /* If the user requested an ephemeral port,
       retrieve the port number assigned by the OS. */
    if(ipaddr_port(addr) == 0) {
        struct ipaddr baddr;
        socklen_t len = sizeof(struct ipaddr);
        rc = getsockname(s, (struct sockaddr*)&baddr, &len);
        if(rc < 0) {err = errno; goto error3;}
        ipaddr_setport(addr, ipaddr_port(&baddr));
    }
    /* Create the object. */
    struct tls_listener *self = (struct tls_listener*)mem;
    self->hvfs.query = tls_listener_hquery;
    self->hvfs.close = tls_listener_hclose;
    self->hvfs.done = NULL;
    self->fd = s;
    self->ctx = ctx;
    self->mem = 1;
    /* Create handle. */
    int h = hmake(&self->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error3;}
    return h;
error3:
    close(s);
error2:
    SSL_CTX_free(ctx);
error1:
    errno = err;
    return -1;
}

int tls_listen(struct ipaddr *addr, const char *cert, const char *pkey,
      int backlog) {
    int err;
    struct tls_listener *obj = malloc(TLS_LISTENER_SIZE);
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int ls = tls_listen_mem(addr, cert, pkey, backlog, obj);
    if(dill_slow(ls < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return ls;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int tls_accept_mem(int s, struct ipaddr *addr, void *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    /* Retrieve the listener object. */
    struct tls_listener *lst = hquery(s, tls_listener_type);
    if(dill_slow(!lst)) {err = errno; goto error1;}
    /* Try to get new connection in a non-blocking way. */
    socklen_t addrlen = sizeof(struct ipaddr);
    int as = fd_accept(lst->fd, (struct sockaddr*)addr, &addrlen, deadline);
    if(dill_slow(as < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fd_unblock(as);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Create OpenSSL connection object. */
    SSL *ssl = SSL_new(lst->ctx);
    if(dill_slow(!ssl)) {err = EFAULT; goto error2;}
    rc = SSL_set_fd(ssl, as);
    if(dill_slow(rc != 1)) {err = EFAULT; goto error3;}
    /* Do the TLS protocol initialization. */
    while(1) {
        errno = 0;
        ERR_clear_error();
        rc = SSL_accept(ssl);
        if(tls_followup(as, ssl, rc, deadline)) break;
    }
    /* Create the handle. */
    int h = tls_makeconn(as, ssl, mem);
    if(dill_slow(h < 0)) {err = errno; goto error3;}
    return h;
error3:
    SSL_free(ssl);
error2:
    fd_close(as);
error1:
    errno = err;
    return -1;
}

int tls_accept(int s, struct ipaddr *addr, int64_t deadline) {
    int err;
    struct tls_conn *obj = malloc(TLS_SIZE);
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int as = tls_accept_mem(s, addr, obj, deadline);
    if(dill_slow(as < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return as;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

static void tls_listener_hclose(struct hvfs *hvfs) {
    struct tls_listener *self = (struct tls_listener*)hvfs;
    fd_close(self->fd);
    SSL_CTX_free(self->ctx);
    if(!self->mem) free(self);
}

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

static void tls_init(void)
{
    static int init = 0;
    if(dill_slow(!init)) {
        SSL_load_error_strings();	
        OpenSSL_add_ssl_algorithms();
        init = 1;
    }
}

static int tls_makeconn(int fd, SSL *ssl, void *mem) {
    int err;
    /* Create the object. */
    struct tls_conn *self = (struct tls_conn*)mem;
    self->hvfs.query = tls_hquery;
    self->hvfs.close = tls_hclose;
    self->hvfs.done = tls_hdone;
    self->bvfs.bsendl = tls_bsendl;
    self->bvfs.brecvl = tls_brecvl;
    self->fd = fd;
    self->ssl = ssl;
    self->indone = 0;
    self->outdone = 0;
    self->inerr = 0;
    self->outerr = 0;
    self->mem = 1;
    /* Create the handle. */
    return hmake(&self->hvfs);
}

/* Do the follow up work after calling a SSL function.
   Returns 0 if the SSL function has to be restarted, 1 is we are done.
   In the latter case, error code is in errno.
   In the case of success errno is set to zero. */
static int tls_followup(int s, SSL *ssl, int rc, int64_t deadline) {
    int err;
    char errstr[120];
    int code = SSL_get_error(ssl, rc);
	  switch(code) {
	  case SSL_ERROR_NONE:
        errno = 0;
        return 1;
	  case SSL_ERROR_ZERO_RETURN:
        errno = EPIPE;
        return 1;
	  case SSL_ERROR_WANT_READ:
        rc = fdin(s, deadline);
        if(dill_slow(rc < 0)) return 1;
        return 0;
    case SSL_ERROR_WANT_WRITE:
        rc = fdout(s, deadline);
        if(dill_slow(rc < 0)) return 1;
        return 0;
    case SSL_ERROR_SYSCALL:
        /* Let's have a look whether there's an error in the error queue. */
        err = ERR_get_error();
        if(err != 0) {
            ERR_error_string(err, errstr);
            fprintf(stderr, "%s\n", errstr);
            errno = EFAULT;
            return 1;
        }
        /* This may be the case during SSL_shutdown. */
        if(rc == 0) {errno = EPIPE; return 1;}
        /* In this case, error is reported via errno. */
        dill_assert(rc == -1);
        /* This is the weird case, but it looks like it happens after
           the connection is closed by the peer. Retrying the operation
           happens to fix the problem. */
        if(errno == 0) return 0;
        return 1;
	  case SSL_ERROR_SSL:
        err = ERR_get_error();
        ERR_error_string(err, errstr);
        fprintf(stderr, "%s\n", errstr);
        errno = EFAULT;
        return 1;
    default:
        fprintf(stderr, "SSL error %d\n", code);
        errno = EFAULT;
        return 1;
    }
}

