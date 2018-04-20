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
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "iol.h"
#include "utils.h"

#define DILL_TLS_BUFSIZE 2048

dill_unique_id(dill_dtls_type);

struct dill_dtls_sock {
    struct dill_hvfs hvfs;
    struct dill_msock_vfs mvfs;
    SSL_CTX *ctx;
    SSL *ssl;
    int u;
    int64_t deadline;
    uint8_t *buf;
    size_t buflen;
    unsigned int busy : 1;
    unsigned int indone : 1;
    unsigned int outdone: 1;
    unsigned int inerr : 1;
    unsigned int outerr : 1;
    unsigned int mem : 1;
};

DILL_CT_ASSERT(sizeof(struct dill_dtls_storage) >=
    sizeof(struct dill_dtls_sock));

static void dill_dtls_init(void);
static BIO *dill_dtls_new_cbio(void *mem);
static int dill_dtls_followup(struct dill_dtls_sock *self, int rc,
    int64_t deadline);

static void *dill_dtls_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_dtls_hclose(struct dill_hvfs *hvfs);
static int dill_dtls_msendl(struct dill_msock_vfs *mvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
static ssize_t dill_dtls_mrecvl(struct dill_msock_vfs *mvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);

static void *dill_dtls_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_dtls_sock *self = (struct dill_dtls_sock*)hvfs;
    if(type == dill_msock_type) return &self->mvfs;
    if(type == dill_dtls_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int dill_dtls_attach_client_mem(int s, struct dill_dtls_storage *mem,
      int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    /* Check whether underlying socket is a bytestream. */
    void *q = dill_hquery(s, dill_msock_type);
    if(dill_slow(!q && errno == ENOTSUP)) {err = EPROTO; goto error1;}
    if(dill_slow(!q)) {err = errno; goto error1;}
    /* Create OpenSSL connection context. */
    dill_dtls_init();
    const SSL_METHOD *method = DTLS_client_method();
    if(dill_slow(!method)) {err = EFAULT; goto error1;}
    SSL_CTX *ctx = SSL_CTX_new(method);
    if(dill_slow(!ctx)) {err = EFAULT; goto error1;}
    /* Create OpenSSL connection object. */
    SSL *ssl = SSL_new(ctx);
    if(dill_slow(!ssl)) {err = EFAULT; goto error2;}
	  SSL_set_connect_state(ssl);
    /* Create a BIO and attach it to the connection. */
    BIO *bio = dill_dtls_new_cbio(mem);
    if(dill_slow(!bio)) {err = errno; goto error3;}
	  SSL_set_bio(ssl, bio, bio);
    /* Take ownership of the underlying socket. */
    s = dill_hown(s);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Create the object. */
    struct dill_dtls_sock *self = (struct dill_dtls_sock*)mem;
    self->hvfs.query = dill_dtls_hquery;
    self->hvfs.close = dill_dtls_hclose;
    self->mvfs.msendl = dill_dtls_msendl;
    self->mvfs.mrecvl = dill_dtls_mrecvl;
    self->ctx = ctx;
    self->ssl = ssl;
    self->u = s;
    self->deadline = -1;
    self->buf = NULL;
    self->buflen = 0;
    self->busy = 0;
    self->indone = 0;
    self->outdone = 0;
    self->inerr = 0;
    self->outerr = 0;
    self->mem = 1;
    /* Initial handshaking. */
    while(1) {
        ERR_clear_error();
        int rc = SSL_connect(ssl);
        if(dill_dtls_followup(self, rc, deadline)) break;
        if(dill_slow(errno != 0)) {err = errno; goto error4;}
    }
    /* Create the handle. */
    int h = dill_hmake(&self->hvfs);
    if(dill_slow(h < 0)) {int err = errno; goto error4;}
    return h;
error4:
    BIO_vfree(bio);
error3:
    SSL_free(ssl);
error2:
    SSL_CTX_free(ctx);
error1:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_dtls_attach_client(int s, int64_t deadline) {
    int err;
    struct dill_dtls_sock *obj = malloc(sizeof(struct dill_dtls_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    s = dill_dtls_attach_client_mem(s, (struct dill_dtls_storage*)obj,
        deadline);
    if(dill_slow(s < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return s;
error2:
    free(obj);
error1:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_dtls_attach_server_mem(int s, const char *cert, const char *pkey,
      struct dill_dtls_storage *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    /* Check whether underlying socket is a bytestream. */
    void *q = dill_hquery(s, dill_msock_type);
    if(dill_slow(!q && errno == ENOTSUP)) {err = EPROTO; goto error1;}
    if(dill_slow(!q)) {err = errno; goto error1;}
    /* Create OpenSSL connection context. */
    dill_dtls_init();
    const SSL_METHOD *method = DTLS_server_method();
    if(dill_slow(!method)) {err = EFAULT; goto error1;}
    SSL_CTX *ctx = SSL_CTX_new(method);
    if(dill_slow(!ctx)) {err = EFAULT; goto error1;}
    int rc = SSL_CTX_use_certificate_file(ctx, cert, SSL_FILETYPE_PEM);
    if(dill_slow(rc <= 0)) {err = EFAULT; goto error2;}
    rc = SSL_CTX_use_PrivateKey_file(ctx, pkey, SSL_FILETYPE_PEM);
    if(dill_slow(rc <= 0 )) {err = EFAULT; goto error2;}
    /* Create OpenSSL connection object. */
    SSL *ssl = SSL_new(ctx);
    if(dill_slow(!ssl)) {err = EFAULT; goto error2;}
	  SSL_set_accept_state(ssl);
    /* Create a BIO and attach it to the connection. */
    BIO *bio = dill_dtls_new_cbio(mem);
    if(dill_slow(!bio)) {err = errno; goto error3;}
	  SSL_set_bio(ssl, bio, bio);
    /* Take ownership of the underlying socket. */
    s = dill_hown(s);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Create the object. */
    struct dill_dtls_sock *self = (struct dill_dtls_sock*)mem;
    self->hvfs.query = dill_dtls_hquery;
    self->hvfs.close = dill_dtls_hclose;
    self->mvfs.msendl = dill_dtls_msendl;
    self->mvfs.mrecvl = dill_dtls_mrecvl;
    self->ctx = ctx;
    self->ssl = ssl;
    self->u = s;
    self->deadline = -1;
    self->buf = NULL;
    self->buflen = 0;
    self->indone = 0;
    self->outdone = 0;
    self->inerr = 0;
    self->outerr = 0;
    self->mem = 1;
    /* Initial handshaking. */
    while(1) {
        ERR_clear_error();
        rc = SSL_accept(ssl);
        if(dill_dtls_followup(self, rc, deadline)) break;
        if(dill_slow(errno != 0)) {err = errno; goto error4;}
    }
    /* Create the handle. */
    int h = dill_hmake(&self->hvfs);
    if(dill_slow(h < 0)) {int err = errno; goto error4;}
    return h;
error4:
    BIO_vfree(bio);
error3:
    SSL_free(ssl);
error2:
    SSL_CTX_free(ctx);
error1:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_dtls_attach_server(int s, const char *cert, const char *pkey,
      int64_t deadline) {
    int err;
    struct dill_dtls_sock *obj = malloc(sizeof(struct dill_dtls_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    s = dill_dtls_attach_server_mem(s, cert, pkey,
        (struct dill_dtls_storage*)obj, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return s;
error2:
    free(obj);
error1:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_dtls_done(int s, int64_t deadline) {
    struct dill_dtls_sock *self = dill_hquery(s, dill_dtls_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    while(1) {
        ERR_clear_error();
        int rc = SSL_shutdown(self->ssl);
        if(rc == 0) {self->outdone = 1; return 0;}
        if(rc == 1) {self->outdone = 1; self->indone = 1; return 0;}
        if(dill_dtls_followup(self, rc, deadline)) break;
    }
    dill_assert(errno != 0);
    self->outerr = 1;
    return -1;
}

int dill_dtls_detach(int s, int64_t deadline) {
    int err;
    struct dill_dtls_sock *self = dill_hquery(s, dill_dtls_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->inerr || self->outerr)) {err = ECONNRESET; goto error;}
    /* Start terminal TLS handshake. */
    if(!self->outdone) {
        int rc = dill_dtls_done(s, deadline);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    /* Wait for the handshake acknowledgement from the peer. */
    if(!self->indone) {
        while(1) {
            ERR_clear_error();
            int rc = SSL_shutdown(self->ssl);
            if(rc == 1) break;
            if(dill_dtls_followup(self, rc, deadline)) {
                err = errno; goto error;}
        }
    }
    int u = self->u;
    self->u = -1;
    dill_dtls_hclose(&self->hvfs);
    return u;
error:
    dill_dtls_hclose(&self->hvfs);
    errno = err;
    return -1;
}

static int dill_dtls_msendl(struct dill_msock_vfs *mvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_dtls_sock *self = dill_cont(mvfs, struct dill_dtls_sock, mvfs);
    if(dill_slow(self->busy)) {errno = EBUSY; return -1;}
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    self->deadline = deadline;
    size_t sz;
    int rc = dill_iolcheck(first, last, NULL, &sz);
    if(dill_slow(rc < 0)) return -1;
    struct dill_iolist iol = *first;
    if(first != last) {
        /* OpenSSL doesn't support iovecs so we have to copy everything into
           a single buffer here. */
        if(sz > self->buflen) {
            self->buf = realloc(self->buf, sz);
            if(!self->buf) {errno = ENOMEM; return -1;}
            self->buflen = sz;
        }
        rc = dill_iolfrom(self->buf, self->buflen, first);
        dill_assert(rc == 0);
        iol.iol_base = self->buf;
        iol.iol_len = sz;
    }
    while(1) {
        ERR_clear_error();
        self->busy = 1;
        int rc = SSL_write(self->ssl, iol.iol_base, iol.iol_len);
        if(!dill_dtls_followup(self, rc, deadline)) continue;
        self->busy = 0;
        if(dill_slow(errno != 0)) {self->outerr = 1; return -1;}
        dill_assert(rc == iol.iol_len);
        break;
    }
    return 0;
}

static ssize_t dill_dtls_mrecvl(struct dill_msock_vfs *mvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_dtls_sock *self = dill_cont(mvfs, struct dill_dtls_sock, mvfs);
    if(dill_slow(self->busy)) {errno = EBUSY; return -1;}
    if(dill_slow(self->indone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->inerr)) {errno = ECONNRESET; return -1;}
    self->deadline = deadline;
    /* TODO: Allow for multiple iolist items. */
    dill_assert(first == last);
    ssize_t res = 0;
    while(1) {
        ERR_clear_error();
        self->busy = 1;
        int rc = SSL_read(self->ssl, first->iol_base, first->iol_len);
        int rc2 = dill_dtls_followup(self, rc, deadline);
        self->busy = 0;
        if(!rc2) {
            res += rc;
            continue;
        }
        if(dill_slow(errno != 0)) {
            if(errno == EPIPE) self->indone = 1;
            else self->inerr = 1;
            return -1;
        }
        res += rc;
        break;
    }
    return res;
}

static void dill_dtls_hclose(struct dill_hvfs *hvfs) {
    struct dill_dtls_sock *self = (struct dill_dtls_sock*)hvfs;
    SSL_free(self->ssl);
    SSL_CTX_free(self->ctx);
    if(dill_fast(self->u >= 0)) {
        int rc = dill_hclose(self->u);
        dill_assert(rc == 0);
    }
    if(self->buf) free(self->buf);
    if(!self->mem) free(self);
}

/******************************************************************************/
/*  Helpers.                                                                  */
/******************************************************************************/

/* Do the follow up work after calling a SSL function.
   Returns 0 if the SSL function has to be restarted, 1 is we are done.
   In the latter case, error code is in errno.
   In the case of success errno is set to zero. */
static int dill_dtls_followup(struct dill_dtls_sock *self, int rc,
      int64_t deadline) {
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
/*  Custom BIO on top of a msock.                                             */
/******************************************************************************/

static BIO_METHOD *dill_dtls_cbio = NULL;

static BIO *dill_dtls_new_cbio(void *mem) {
    BIO *bio = BIO_new(dill_dtls_cbio);
    if(dill_slow(!bio)) {errno = EFAULT; return NULL;}
    BIO_set_data(bio, mem);
    BIO_set_init(bio, 1);
    return bio;
}

static int dill_dtls_cbio_create(BIO *bio) {
    return 1;
}

static int dill_dtls_cbio_destroy(BIO *bio) {
    return 1;
}

static int dill_dtls_cbio_write(BIO *bio, const char *buf, int len) {
    struct dill_dtls_sock *self = (struct dill_dtls_sock*)BIO_get_data(bio);
    int rc = dill_msend(self->u, buf, len, self->deadline);
    if(dill_slow(rc < 0)) return -1;
    return len;
}

static int dill_dtls_cbio_read(BIO *bio, char *buf, int len) {
    struct dill_dtls_sock *self = (struct dill_dtls_sock*)BIO_get_data(bio);
    ssize_t sz = dill_mrecv(self->u, buf, len, self->deadline);
    if(dill_slow(sz < 0)) return -1;
    return (int)sz;
}

static long dill_dtls_cbio_ctrl(BIO *bio, int cmd, long larg, void *parg) {
    switch(cmd) {
    case BIO_CTRL_FLUSH:
        return 1;
    case BIO_CTRL_PUSH:
    case BIO_CTRL_POP:
        return 0;
    case BIO_CTRL_DGRAM_SET_NEXT_TIMEOUT:;
        struct dill_dtls_sock *self = (struct dill_dtls_sock*)BIO_get_data(bio);
        struct timeval *dd = (struct timeval*)parg;
        struct timeval nw;
        gettimeofday(&nw, NULL);
        int64_t delay = nw.tv_sec - dd->tv_sec;
        if(delay < 0) delay = 0;
        self->deadline = dill_now() + (delay * 1000);
        break;
    case BIO_CTRL_DGRAM_GET_MTU_OVERHEAD:
        return 0; /* ? */
    case BIO_CTRL_DGRAM_QUERY_MTU:
        return 1000; /* ? */
    case BIO_CTRL_WPENDING:
        return 0;
    default:
        dill_assert(0);
    }
}

static void dill_dtls_term(void) {
    BIO_meth_free(dill_dtls_cbio);
    ERR_free_strings();
    EVP_cleanup();
}

static void dill_dtls_init(void) {
    static int init = 0;
    if(dill_slow(!init)) {
        SSL_library_init();
        SSL_load_error_strings();
        /* Create our own custom BIO type. */
        int idx = BIO_get_new_index();
        dill_dtls_cbio = BIO_meth_new(idx, "msock");
        dill_assert(dill_dtls_cbio);
        int rc = BIO_meth_set_create(dill_dtls_cbio, dill_dtls_cbio_create);
        dill_assert(rc == 1);
        rc = BIO_meth_set_destroy(dill_dtls_cbio, dill_dtls_cbio_destroy);
        dill_assert(rc == 1);
        rc = BIO_meth_set_write(dill_dtls_cbio, dill_dtls_cbio_write);
        dill_assert(rc == 1);
        rc = BIO_meth_set_read(dill_dtls_cbio, dill_dtls_cbio_read);
        dill_assert(rc == 1);
        rc = BIO_meth_set_ctrl(dill_dtls_cbio, dill_dtls_cbio_ctrl);
        dill_assert(rc == 1);
        /* Deallocate the method once the process exits. */
        rc = atexit(dill_dtls_term);
        dill_assert(rc == 0);
        init = 1;
    }
}

