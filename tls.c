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
#include <libdillimpl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdint.h>
#include <stdlib.h>

#include "utils.h"

#define TLS_BUFSIZE 2048

dill_unique_id(tls_type);

struct tls_sock {
    struct hvfs hvfs;
    struct bsock_vfs bvfs;
    SSL_CTX *ctx;
    SSL *ssl;
    int u;
    int64_t deadline;
    unsigned int indone : 1;
    unsigned int outdone: 1;
    unsigned int inerr : 1;
    unsigned int outerr : 1;
    unsigned int mem : 1;
};

DILL_CT_ASSERT(sizeof(struct tls_storage) >= sizeof(struct tls_sock));

static void tls_init(void);
static BIO *tls_new_cbio(void *mem);
static int tls_followup(struct tls_sock *self, int rc, int64_t deadline);

static void *tls_hquery(struct hvfs *hvfs, const void *type);
static void tls_hclose(struct hvfs *hvfs);
static int tls_bsendl(struct bsock_vfs *bvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static int tls_brecvl(struct bsock_vfs *bvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

static void *tls_hquery(struct hvfs *hvfs, const void *type) {
    struct tls_sock *self = (struct tls_sock*)hvfs;
    if(type == bsock_type) return &self->bvfs;
    if(type == tls_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int tls_attach_client_mem(int s, struct tls_storage *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    /* Check whether underlying socket is a bytestream. */
    void *q = hquery(s, bsock_type);
    if(dill_slow(!q && errno == ENOTSUP)) {err = EPROTO; goto error1;}
    if(dill_slow(!q)) {err = errno; goto error1;}
    /* Create OpenSSL connection context. */
    tls_init();
    const SSL_METHOD *method = SSLv23_method();
    if(dill_slow(!method)) {err = EFAULT; goto error1;}
    SSL_CTX *ctx = SSL_CTX_new(method);
    if(dill_slow(!ctx)) {err = EFAULT; goto error1;}
    /* Create OpenSSL connection object. */
    SSL *ssl = SSL_new(ctx);
    if(dill_slow(!ssl)) {err = EFAULT; goto error2;}
	  SSL_set_connect_state(ssl);
    /* Create a BIO and attach it to the connection. */
    BIO *bio = tls_new_cbio(mem);
    if(dill_slow(!bio)) {err = errno; goto error3;}
	  SSL_set_bio(ssl, bio, bio);
    /* Make a private copy of the underlying socket. */
    int u = hdup(s);
    if(dill_slow(u < 0)) {err = errno; goto error4;}
    int rc = hclose(s);
    dill_assert(rc == 0);
    s = u;
    /* Create the object. */
    struct tls_sock *self = (struct tls_sock*)mem;
    self->hvfs.query = tls_hquery;
    self->hvfs.close = tls_hclose;
    self->hvfs.done = NULL;
    self->bvfs.bsendl = tls_bsendl;
    self->bvfs.brecvl = tls_brecvl;
    self->ctx = ctx;
    self->ssl = ssl;
    self->u = u;
    self->deadline = -1;
    self->indone = 0;
    self->outdone = 0;
    self->inerr = 0;
    self->outerr = 0;
    self->mem = 1;
    /* Initial handshaking. */
    while(1) {
        ERR_clear_error();
        rc = SSL_connect(ssl);
        if(tls_followup(self, rc, deadline)) break;
        if(dill_slow(errno != 0)) {err = errno; goto error4;}
    }
    /* Create the handle. */
    int h = hmake(&self->hvfs);
    if(dill_slow(h < 0)) {int err = errno; goto error4;}
    return h;
error4:
    BIO_vfree(bio);
error3:
    SSL_free(ssl);
error2:
    SSL_CTX_free(ctx);
error1:
    rc = hclose(s);
    dill_assert(rc == 0);
    errno = err;
    return -1;
}

int tls_attach_client(int s, int64_t deadline) {
    int err;
    struct tls_sock *obj = malloc(sizeof(struct tls_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int ts = tls_attach_client_mem(s, (struct tls_storage*)obj, deadline);
    if(dill_slow(ts < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return ts;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int tls_attach_server_mem(int s, const char *cert, const char *pkey,
      struct tls_storage *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    /* Check whether underlying socket is a bytestream. */
    void *q = hquery(s, bsock_type);
    if(dill_slow(!q && errno == ENOTSUP)) {err = EPROTO; goto error1;}
    if(dill_slow(!q)) {err = errno; goto error1;}
    /* Create OpenSSL connection context. */
    tls_init();
    const SSL_METHOD *method = SSLv23_server_method();
    if(dill_slow(!method)) {err = EFAULT; goto error1;}
    SSL_CTX *ctx = SSL_CTX_new(method);
    if(dill_slow(!ctx)) {err = EFAULT; goto error1;}
    int rc = SSL_CTX_set_ecdh_auto(ctx, 1);
    if(dill_slow(rc != 1)) {err = EFAULT; goto error2;}
    rc = SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM);
    if(dill_slow(rc <= 0)) {err = EFAULT; goto error2;}
    rc = SSL_CTX_use_PrivateKey_file(ctx, pkey, SSL_FILETYPE_PEM);
    if(dill_slow(rc <= 0 )) {err = EFAULT; goto error2;}
    /* Create OpenSSL connection object. */
    SSL *ssl = SSL_new(ctx);
    if(dill_slow(!ssl)) {err = EFAULT; goto error2;}
	  SSL_set_accept_state(ssl);
    /* Create a BIO and attach it to the connection. */
    BIO *bio = tls_new_cbio(mem);
    if(dill_slow(!bio)) {err = errno; goto error3;}
	  SSL_set_bio(ssl, bio, bio);
    /* Make a private copy of the underlying socket. */
    int u = hdup(s);
    if(dill_slow(u < 0)) {err = errno; goto error4;}
    rc = hclose(s);
    dill_assert(rc == 0);
    s = u;
    /* Create the object. */
    struct tls_sock *self = (struct tls_sock*)mem;
    self->hvfs.query = tls_hquery;
    self->hvfs.close = tls_hclose;
    self->hvfs.done = NULL;
    self->bvfs.bsendl = tls_bsendl;
    self->bvfs.brecvl = tls_brecvl;
    self->ctx = ctx;
    self->ssl = ssl;
    self->u = u;
    self->deadline = -1;
    self->indone = 0;
    self->outdone = 0;
    self->inerr = 0;
    self->outerr = 0;
    self->mem = 1;
    /* Initial handshaking. */
    while(1) {
        ERR_clear_error();
        rc = SSL_accept(ssl);
        if(tls_followup(self, rc, deadline)) break;
        if(dill_slow(errno != 0)) {err = errno; goto error4;}
    }
    /* Create the handle. */
    int h = hmake(&self->hvfs);
    if(dill_slow(h < 0)) {int err = errno; goto error4;}
    return h;
error4:
    BIO_vfree(bio);
error3:
    SSL_free(ssl);
error2:
    SSL_CTX_free(ctx);
error1:
    rc = hclose(s);
    dill_assert(rc == 0);
    errno = err;
    return -1;
}

int tls_attach_server(int s, const char *cert, const char *pkey,
      int64_t deadline) {
    int err;
    struct tls_sock *obj = malloc(sizeof(struct tls_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int ts = tls_attach_server_mem(s, cert, pkey, (struct tls_storage*)obj,
        deadline);
    if(dill_slow(ts < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return ts;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int tls_done(int s, int64_t deadline) {
    struct tls_sock *self = hquery(s, tls_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    while(1) {
        ERR_clear_error();
        int rc = SSL_shutdown(self->ssl);
        if(rc == 0) {self->outdone = 1; return 0;}
        if(rc == 1) {self->outdone = 1; self->indone = 1; return 0;}
        if(tls_followup(self, rc, deadline)) break;
    }
    dill_assert(errno != 0);
    self->outerr = 1;
    return -1;
}

int tls_detach(int s, int64_t deadline) {
    int err;
    struct tls_sock *self = hquery(s, tls_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->inerr || self->outerr)) {err = ECONNRESET; goto error;}
    /* Start terminal TLS handshake. */
    if(!self->outdone) {
        int rc = tls_done(s, deadline);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    /* Wait for the handshake acknowledgement from the peer. */
    if(!self->indone) {
        while(1) {
            ERR_clear_error();
            int rc = SSL_shutdown(self->ssl);
            if(rc == 1) break;
            if(tls_followup(self, rc, deadline)) {err = errno; goto error;}
        }
    }
    int u = self->u;
    self->u = -1;
    tls_hclose(&self->hvfs);
    return u;
error:
    tls_hclose(&self->hvfs);
    errno = err;
    return -1;
}

static int tls_bsendl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct tls_sock *self = dill_cont(bvfs, struct tls_sock, bvfs);
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    self->deadline = deadline;
    struct iolist *it = first;
    while(1) {
        uint8_t *base = it->iol_base;
        size_t len = it->iol_len;
        while(1) {
            ERR_clear_error();
            int rc = SSL_write(self->ssl, base, len);
            if(tls_followup(self, rc, deadline)) {
                if(dill_slow(errno != 0)) {
                    self->outerr = 1;
                    return -1;
                }
                if(rc == len) break;
                base += rc;
                len -= rc;
            }
        }
        if(it == last) break;
        it = it->iol_next;
    }
    return 0;
}

static int tls_brecvl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct tls_sock *self = dill_cont(bvfs, struct tls_sock, bvfs);
    if(dill_slow(self->indone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->inerr)) {errno = ECONNRESET; return -1;}
    self->deadline = deadline;
    struct iolist *it = first;
    while(1) {
        uint8_t *base = it->iol_base;
        size_t len = it->iol_len;
        while(1) {
            ERR_clear_error();
            int rc = SSL_read(self->ssl, base, len);
            if(tls_followup(self, rc, deadline)) {
                if(dill_slow(errno != 0)) {
                    if(errno == EPIPE) self->indone = 1;
                    else self->inerr = 1;
                    return -1;
                }
                if(rc == len) break;
                base += rc;
                len -= rc;
            }
        }
        if(it == last) break;
        it = it->iol_next;
    }
    return 0;
}

static void tls_hclose(struct hvfs *hvfs) {
    struct tls_sock *self = (struct tls_sock*)hvfs;
    SSL_free(self->ssl);
    SSL_CTX_free(self->ctx);
    if(dill_fast(self->u >= 0)) {
        int rc = hclose(self->u);
        dill_assert(rc == 0);
    }
    free(self);
}

/******************************************************************************/
/*  Helpers.                                                                  */
/******************************************************************************/

/* Do the follow up work after calling a SSL function.
   Returns 0 if the SSL function has to be restarted, 1 is we are done.
   In the latter case, error code is in errno.
   In the case of success errno is set to zero. */
static int tls_followup(struct tls_sock *self, int rc, int64_t deadline) {
    int err;
    char errstr[120];
    int code = SSL_get_error(self->ssl, rc);
	  switch(code) {
	  case SSL_ERROR_NONE:
        /* Operation finished. */
        errno = 0;
        return 1;
	  case SSL_ERROR_ZERO_RETURN:
        /* Connection terminated by peer. */
        errno = EPIPE;
        return 1;
    case SSL_ERROR_SYSCALL:
        /* Error from our custom BIO. */
        dill_assert(rc == -1);
        if(errno == 0) return 0;
        return 1;
	  case SSL_ERROR_SSL:
        /* SSL errors. Not clear how to convert them into errnos. */
        err = ERR_get_error();
        ERR_error_string(err, errstr);
        fprintf(stderr, "SSL error: %s\n", errstr);
        errno = EFAULT;
        return 1;
	  case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        /* These two should never happen -- our custom BIO is blocking. */
        dill_assert(0);
    default:
        fprintf(stderr, "SSL error %d\n", code);
        dill_assert(0);
    }
}

/******************************************************************************/
/*  Custom BIO on top of a bsock.                                             */
/******************************************************************************/

static BIO_METHOD *tls_cbio = NULL;

static BIO *tls_new_cbio(void *mem) {
    BIO *bio = BIO_new(tls_cbio);
    if(dill_slow(!bio)) {errno = EFAULT; return NULL;}
    BIO_set_data(bio, mem);
    BIO_set_init(bio, 1);
    return bio;
}

static int tls_cbio_create(BIO *bio) {
    return 1;
}

static int tls_cbio_destroy(BIO *bio) {
    return 1;
}

static int tls_cbio_write(BIO *bio, const char *buf, int len) {
    struct tls_sock *self = (struct tls_sock*)BIO_get_data(bio);
    int rc = bsend(self->u, buf, len, self->deadline);
    if(dill_slow(rc < 0)) return -1;
    return len;
}

static int tls_cbio_read(BIO *bio, char *buf, int len) {
    struct tls_sock *self = (struct tls_sock*)BIO_get_data(bio);
    int rc = brecv(self->u, buf, len, self->deadline);
    if(dill_slow(rc < 0)) return -1;
    return len;
}

static long tls_cbio_ctrl(BIO *bio, int cmd, long larg, void *parg) {
    switch(cmd) {
    case BIO_CTRL_FLUSH:
        return 1;
    case BIO_CTRL_PUSH:
    case BIO_CTRL_POP:
        return 0;
    default:
        dill_assert(0);
    }
}

static void tls_term(void) {
    BIO_meth_free(tls_cbio);
    ERR_free_strings();
    EVP_cleanup();
}

static void tls_init(void)
{
    static int init = 0;
    if(dill_slow(!init)) {
        SSL_library_init();
        SSL_load_error_strings();
        /* Create our own custom BIO type. */
        int idx = BIO_get_new_index();
        tls_cbio = BIO_meth_new(idx, "bsock");
        dill_assert(tls_cbio);
        int rc = BIO_meth_set_create(tls_cbio, tls_cbio_create);
        dill_assert(rc == 1);
        rc = BIO_meth_set_destroy(tls_cbio, tls_cbio_destroy);
        dill_assert(rc == 1);
        rc = BIO_meth_set_write(tls_cbio, tls_cbio_write);
        dill_assert(rc == 1);
        rc = BIO_meth_set_read(tls_cbio, tls_cbio_read);
        dill_assert(rc == 1);
        rc = BIO_meth_set_ctrl(tls_cbio, tls_cbio_ctrl);
        dill_assert(rc == 1);
        /* Deallocate the method once the process exits. */
        rc = atexit(tls_term);
        dill_assert(rc == 0);
        init = 1;
    }
}

