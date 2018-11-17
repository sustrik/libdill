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

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "utils.h"

#define DILL_TLS_BUFSIZE 2048

dill_unique_id(dill_tls_type);

struct dill_tls_sock {
    struct dill_hvfs hvfs;
    struct dill_bsock_vfs bvfs;
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

DILL_CHECK_STORAGE(dill_tls_sock, dill_tls_storage)

static void dill_tls_init(void);
static BIO *dill_tls_new_cbio(void *mem);
static int dill_tls_followup(struct dill_tls_sock *self, int rc);

static void *dill_tls_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_tls_hclose(struct dill_hvfs *hvfs);
static int dill_tls_bsendl(struct dill_bsock_vfs *bvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
static int dill_tls_brecvl(struct dill_bsock_vfs *bvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);

static void *dill_tls_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_tls_sock *self = (struct dill_tls_sock*)hvfs;
    if(type == dill_bsock_type) return &self->bvfs;
    if(type == dill_tls_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int dill_tls_attach_client_mem(int s, struct dill_tls_storage *mem,
      int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    /* Check whether underlying socket is a bytestream. */
    void *q = dill_hquery(s, dill_bsock_type);
    if(dill_slow(!q && errno == ENOTSUP)) {err = EPROTO; goto error1;}
    if(dill_slow(!q)) {err = errno; goto error1;}
    /* Create OpenSSL connection context. */
    dill_tls_init();
    const SSL_METHOD *method = SSLv23_method();
    if(dill_slow(!method)) {err = EFAULT; goto error1;}
    SSL_CTX *ctx = SSL_CTX_new(method);
    if(dill_slow(!ctx)) {err = EFAULT; goto error1;}
    /* Create OpenSSL connection object. */
    SSL *ssl = SSL_new(ctx);
    if(dill_slow(!ssl)) {err = EFAULT; goto error2;}
	  SSL_set_connect_state(ssl);
    /* Create a BIO and attach it to the connection. */
    BIO *bio = dill_tls_new_cbio(mem);
    if(dill_slow(!bio)) {err = errno; goto error3;}
	  SSL_set_bio(ssl, bio, bio);
    /* Take ownership of the underlying socket. */
    s = dill_hown(s);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Create the object. */
    struct dill_tls_sock *self = (struct dill_tls_sock*)mem;
    self->hvfs.query = dill_tls_hquery;
    self->hvfs.close = dill_tls_hclose;
    self->bvfs.bsendl = dill_tls_bsendl;
    self->bvfs.brecvl = dill_tls_brecvl;
    self->ctx = ctx;
    self->ssl = ssl;
    self->u = s;
    self->deadline = -1;
    self->indone = 0;
    self->outdone = 0;
    self->inerr = 0;
    self->outerr = 0;
    self->mem = 1;
    /* Initial handshaking. */
    while(1) {
        ERR_clear_error();
        int rc = SSL_connect(ssl);
        if(dill_tls_followup(self, rc)) break;
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

int dill_tls_attach_client(int s, int64_t deadline) {
    int err;
    struct dill_tls_sock *obj = malloc(sizeof(struct dill_tls_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    s = dill_tls_attach_client_mem(s, (struct dill_tls_storage*)obj,
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

int dill_tls_attach_server_mem(int s, const char *cert, const char *pkey,
      struct dill_tls_storage *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    /* Check whether underlying socket is a bytestream. */
    void *q = dill_hquery(s, dill_bsock_type);
    if(dill_slow(!q && errno == ENOTSUP)) {err = EPROTO; goto error1;}
    if(dill_slow(!q)) {err = errno; goto error1;}
    /* Create OpenSSL connection context. */
    dill_tls_init();
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
    BIO *bio = dill_tls_new_cbio(mem);
    if(dill_slow(!bio)) {err = errno; goto error3;}
	  SSL_set_bio(ssl, bio, bio);
    /* Take ownership of the underlying socket. */
    s = dill_hown(s);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Create the object. */
    struct dill_tls_sock *self = (struct dill_tls_sock*)mem;
    self->hvfs.query = dill_tls_hquery;
    self->hvfs.close = dill_tls_hclose;
    self->bvfs.bsendl = dill_tls_bsendl;
    self->bvfs.brecvl = dill_tls_brecvl;
    self->ctx = ctx;
    self->ssl = ssl;
    self->u = s;
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
        if(dill_tls_followup(self, rc)) break;
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

int dill_tls_attach_server(int s, const char *cert, const char *pkey,
      int64_t deadline) {
    int err;
    struct dill_tls_sock *obj = malloc(sizeof(struct dill_tls_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    s = dill_tls_attach_server_mem(s, cert, pkey,
        (struct dill_tls_storage*)obj, deadline);
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

int dill_tls_done(int s, int64_t deadline) {
    struct dill_tls_sock *self = dill_hquery(s, dill_tls_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    while(1) {
        ERR_clear_error();
        int rc = SSL_shutdown(self->ssl);
        if(rc == 0) {self->outdone = 1; return 0;}
        if(rc == 1) {self->outdone = 1; self->indone = 1; return 0;}
        if(dill_tls_followup(self, rc)) break;
    }
    dill_assert(errno != 0);
    self->outerr = 1;
    return -1;
}

int dill_tls_detach(int s, int64_t deadline) {
    int err;
    struct dill_tls_sock *self = dill_hquery(s, dill_tls_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->inerr || self->outerr)) {err = ECONNRESET; goto error;}
    /* Start terminal TLS handshake. */
    if(!self->outdone) {
        int rc = dill_tls_done(s, deadline);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    /* Wait for the handshake acknowledgement from the peer. */
    if(!self->indone) {
        while(1) {
            ERR_clear_error();
            int rc = SSL_shutdown(self->ssl);
            if(rc == 1) break;
            if(dill_tls_followup(self, rc)) {err = errno; goto error;}
        }
    }
    int u = self->u;
    self->u = -1;
    dill_tls_hclose(&self->hvfs);
    return u;
error:
    dill_tls_hclose(&self->hvfs);
    errno = err;
    return -1;
}

static int dill_tls_bsendl(struct dill_bsock_vfs *bvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_tls_sock *self = dill_cont(bvfs, struct dill_tls_sock, bvfs);
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    self->deadline = deadline;
    struct dill_iolist *it = first;
    while(1) {
        uint8_t *base = it->iol_base;
        size_t len = it->iol_len;
        while(len > 0) {
            ERR_clear_error();
            int rc = SSL_write(self->ssl, base, len);
            if(dill_tls_followup(self, rc)) {
                if(dill_slow(errno != 0)) {
                    self->outerr = 1;
                    return -1;
                }
                base += rc;
                len -= rc;
            }
        }
        if(it == last) break;
        it = it->iol_next;
    }
    return 0;
}

static int dill_tls_brecvl(struct dill_bsock_vfs *bvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_tls_sock *self = dill_cont(bvfs, struct dill_tls_sock, bvfs);
    if(dill_slow(self->indone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->inerr)) {errno = ECONNRESET; return -1;}
    self->deadline = deadline;
    struct dill_iolist *it = first;
    while(1) {
        uint8_t *base = it->iol_base;
        size_t len = it->iol_len;
        while(1) {
            ERR_clear_error();
            int rc = SSL_read(self->ssl, base, len);
            if(dill_tls_followup(self, rc)) {
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

static void dill_tls_hclose(struct dill_hvfs *hvfs) {
    struct dill_tls_sock *self = (struct dill_tls_sock*)hvfs;
    SSL_free(self->ssl);
    SSL_CTX_free(self->ctx);
    if(dill_fast(self->u >= 0)) {
        int rc = dill_hclose(self->u);
        dill_assert(rc == 0);
    }
    if(!self->mem) free(self);
}

/******************************************************************************/
/*  Helpers.                                                                  */
/******************************************************************************/

/* OpenSSL has huge amount of underspecified errors. It's hard to deal
   with them in a consistent manner. Morever, you can get multiple of
   them at the same time. There's nothing better to do than to print them to
   the stderr and return generic EFAULT error instead. */
static void dill_tls_process_errors(void) {
    char errstr[512];
    while(1) {
        int err = ERR_get_error();
        if(err == 0) break;
        ERR_error_string_n(err, errstr, sizeof(errstr));
        fprintf(stderr, "SSL error: %s\n", errstr);
    }
    errno = EFAULT;
}

/* Do the follow up work after calling a SSL function.
   Returns 0 if the SSL function has to be restarted, 1 is we are done.
   In the latter case, error code is in errno.
   In the case of success errno is set to zero. */
static int dill_tls_followup(struct dill_tls_sock *self, int rc) {
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
        dill_tls_process_errors();
        return 1;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        /* These two should never happen -- our custom BIO is blocking. */
        dill_assert(0);
    default:
        /* Unexpected error. Let's at least print out current error queue
           for debugging purposes. */
        fprintf(stderr, "SSL error code: %d\n", code);
        dill_tls_process_errors();
        dill_assert(0);
    }
}

/******************************************************************************/
/*  Custom BIO on top of a bsock.                                             */
/******************************************************************************/

static BIO_METHOD *dill_tls_cbio = NULL;

static BIO *dill_tls_new_cbio(void *mem) {
    BIO *bio = BIO_new(dill_tls_cbio);
    if(dill_slow(!bio)) {errno = EFAULT; return NULL;}
    BIO_set_data(bio, mem);
    BIO_set_init(bio, 1);
    return bio;
}

static int dill_tls_cbio_create(BIO *bio) {
    return 1;
}

static int dill_tls_cbio_destroy(BIO *bio) {
    return 1;
}

static int dill_tls_cbio_write(BIO *bio, const char *buf, int len) {
    struct dill_tls_sock *self = (struct dill_tls_sock*)BIO_get_data(bio);
    int rc = dill_bsend(self->u, buf, len, self->deadline);
    if(dill_slow(rc < 0)) return -1;
    return len;
}

static int dill_tls_cbio_read(BIO *bio, char *buf, int len) {
    struct dill_tls_sock *self = (struct dill_tls_sock*)BIO_get_data(bio);
    int rc = dill_brecv(self->u, buf, len, self->deadline);
    if(dill_slow(rc < 0)) return -1;
    return len;
}

static long dill_tls_cbio_ctrl(BIO *bio, int cmd, long larg, void *parg) {
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

static void dill_tls_term(void) {
    BIO_meth_free(dill_tls_cbio);
    ERR_free_strings();
    EVP_cleanup();
}

static void dill_tls_init(void) {
    static int init = 0;
    if(dill_slow(!init)) {
        SSL_library_init();
        SSL_load_error_strings();
        /* Create our own custom BIO type. */
        int idx = BIO_get_new_index();
        dill_tls_cbio = BIO_meth_new(idx, "bsock");
        dill_assert(dill_tls_cbio);
        int rc = BIO_meth_set_create(dill_tls_cbio, dill_tls_cbio_create);
        dill_assert(rc == 1);
        rc = BIO_meth_set_destroy(dill_tls_cbio, dill_tls_cbio_destroy);
        dill_assert(rc == 1);
        rc = BIO_meth_set_write(dill_tls_cbio, dill_tls_cbio_write);
        dill_assert(rc == 1);
        rc = BIO_meth_set_read(dill_tls_cbio, dill_tls_cbio_read);
        dill_assert(rc == 1);
        rc = BIO_meth_set_ctrl(dill_tls_cbio, dill_tls_cbio_ctrl);
        dill_assert(rc == 1);
        /* Deallocate the method once the process exits. */
        rc = atexit(dill_tls_term);
        dill_assert(rc == 0);
        init = 1;
    }
}
