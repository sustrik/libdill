/*

  Copyright (c) 2021 Tim Hewitt

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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DILL_DISABLE_RAW_NAMES
#include <libdillimpl.h>
#include "iol.h"
#include "utils.h"

#define DILL_MAX_TERMINATOR_LENGTH 32

dill_unique_id(dill_hup_type);

static void *
dill_hup_hquery(struct dill_hvfs *hvfs, const void *type);

static void
dill_hup_hclose(struct dill_hvfs *hvfs);

static int
dill_hup_msendl(
    struct dill_msock_vfs *mvfs,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline
);

static ssize_t
dill_hup_mrecvl(
    struct dill_msock_vfs *mvfs,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline
);

struct dill_hup_sock {
    struct dill_hvfs hvfs;
    struct dill_msock_vfs mvfs;
    int under;
    size_t len;
    uint8_t buf[DILL_MAX_TERMINATOR_LENGTH];
    unsigned int indone: 1;
    unsigned int outdone: 1;
    unsigned int sent: 1;
    unsigned int ownmem : 1;
};

DILL_CHECK_STORAGE(dill_hup_sock, dill_hup_storage)

#define lest(cond) if (dill_slow(cond))

static void *
dill_hup_hquery(struct dill_hvfs *hvfs, const void *type)
{
    struct dill_hup_sock *this = (struct dill_hup_sock *)hvfs;
    if (type == dill_msock_type) return &this->mvfs;
    if (type == dill_hup_type) return this;
    errno = ENOTSUP;
    return NULL;
}

int dill_hup_attach_mem(
    int s,
    const void *buf,
    size_t len,
    struct dill_hup_storage *mem
)
{
    int err;

    /* Do we actually have memory and a small enough terminator? */
    lest (!mem || len > DILL_MAX_TERMINATOR_LENGTH) {
        err = EINVAL;
        goto error;
    }

    /* Do we actually have a terminator? */
    lest (len > 0 && !buf) {
        err = EINVAL;
        goto error;
    }

    /* claim the socket */
    s = dill_hown(s);
    lest (s < 0) {
        err = errno;
        goto error;
    }

    /* make sure it's an msocket */
    void *q = dill_hquery(s, dill_msock_type);
    lest (!q) {
        err = errno == ENOTSUP? EPROTO : errno;
        goto error;
    }

    /* make the darn thing! */
    struct dill_hup_sock *this = (struct dill_hup_sock *)mem;
    this->hvfs.query = dill_hup_hquery;
    this->hvfs.close = dill_hup_hclose;
    this->mvfs.msendl = dill_hup_msendl;
    this->mvfs.mrecvl = dill_hup_mrecvl;
    this->under = s;
    this->len = len;
    memcpy(this->buf, buf, len);
    this->outdone = 0;
    this->indone = 0;
    this->sent = 0;
    this->ownmem = 0;

    /* wrap it in a handle */
    int h = dill_hmake(&this->hvfs);
    lest (h < 0) {
        err = errno;
        goto error;
    }

    return h;

error:
    if (s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int
dill_hup_attach(int s, const void *buf, size_t len)
{
    int err;

    struct dill_hup_sock *obj = malloc(sizeof(struct dill_hup_sock));
    lest (!obj) {
        err = ENOMEM;
        goto error1;
    }

    s = dill_hup_attach_mem(s, buf, len, (struct dill_hup_storage *)obj);
    lest (s < 0) {
        err = errno;
        goto error2;
    }

    obj->ownmem = 1;
    return s;

error2:
    free(obj);
error1:
    if (s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int
dill_hup_done(int s, int64_t deadline)
{
    struct dill_hup_sock *this = dill_hquery(s, dill_hup_type);
    lest (!this) return -1;

    lest (this->outdone) {
        errno = EPIPE;
        return -1;
    }

    int rc = dill_msend(this->under, this->buf, this->len, deadline);
    lest (rc < 0) return -1;

    this->outdone = 1;
    return 0;
}

int
dill_hup_detach(int s, int64_t deadline)
{
    int err;

    struct dill_hup_sock *this = dill_hquery(s, dill_hup_type);
    lest(!this) return -1;

    /* Only need to hang up if we sent anything */
    if (this->sent && !this->outdone) {
        int rc = dill_hup_done(s, deadline);
        lest (rc < 0) {
            err = errno;
            goto error;
        }
    }
    s = this->under;
    if (this->ownmem) free(this);
    return s;

error: /* a declaration is not a statement, but a semicolon is -> */;
    int rc = dill_hclose(s);
    errno = err;
    return -1;
}

static int
dill_hup_msendl(
    struct dill_msock_vfs *mvfs,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline
)
{
    struct dill_hup_sock *this = dill_cont(mvfs, struct dill_hup_sock, mvfs);

    /* A peer cannot send a HUP message after they have claimed to hang up */
    lest (this->outdone) {
        errno = EPIPE;
        return -1;
    }

    /* TODO: compare msg to HUP msg
     * (utility function to compare iolists to buffers would be nice...)
     */
    int rc = dill_msendl(this->under, first, last, deadline);
    if (dill_fast(rc >= 0)) this->sent = 1;
    return rc;
}

static ssize_t
dill_hup_mrecvl(
    struct dill_msock_vfs *mvfs,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline
)
{
    struct dill_hup_sock *this = dill_cont(mvfs, struct dill_hup_sock, mvfs);

    /* A peer will not send any more HUP messages after they claim to be done,
     * but they may be sending over another protocol, so we must not even
     * attempt to read.
     */
    lest (this->indone) {
        errno = EPIPE;
        return -1;
    }

    if (this->len == 0) {
        ssize_t sz = dill_mrecvl(this->under, first, last, deadline);
        lest (sz < 0) return -1;
        lest (sz == 0) {
            this->indone = 1;
            errno = EPIPE;
            return -1;
        }
        return sz;
    }

    /*
     * Temporarily replace the first this->len bytes in the iolist with a local
     * buffer so we can easily compare them with memcmp.
     *
     * (That buffer-iolist compare function sounds pretty handy...)
     */
    struct dill_iolist trimmed = {0};
    int rc = dill_ioltrim(first, this->len, &trimmed);
    uint8_t buf[this->len];
    struct dill_iolist iol = {buf, this->len, rc < 0? NULL : &trimmed, 0};

    ssize_t sz = dill_mrecvl(this->under, &iol, rc < 0? &iol : last, deadline);
    lest (sz < 0) return -1;
    lest (sz == this->len && memcmp(this->buf, buf, this->len) == 0) {
        this->indone = 1;
        errno = EPIPE;
        return -1;
    }

    dill_iolto(buf, this->len, first);
    return sz;
}

static void
dill_hup_hclose(struct dill_hvfs *hvfs)
{
    struct dill_hup_sock *this = (struct dill_hup_sock *)hvfs;

    if (dill_fast(this->under >= 0)) {
        int rc = dill_hclose(this->under);
        dill_assert(rc == 0);
    }

    if (this->ownmem) free(this);
}
