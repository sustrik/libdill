/*

  Copyright (c) 2016 Martin Sustrik

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
#include <stddef.h>
#include <stdlib.h>

#include "libdill.h"
#include "utils.h"

struct dill_hndl {
    void *type;
    void *data;
    hndlstop_fn stop;
    int done;
    int next;
};

static struct dill_hndl *dill_hndls = NULL;
static int dill_nhndls = 0;
static int dill_unused = -1;

int hndl(void *type, void *data, hndlstop_fn stop) {
    /* If there's no space for the new handle expand the array. */
    if(dill_slow(dill_unused == -1)) {
        /* Start with 256 handles, double the size when needed. */
        int sz = dill_nhndls ? dill_nhndls * 2 : 256;
        struct dill_hndl *hndls =
            realloc(dill_hndls, sz * sizeof(struct dill_hndl));
        if(dill_slow(!hndls)) {errno = ENOMEM; return -1;}
        /* Add newly allocated handles to the list of unused handles. */
        int i;
        for(i = dill_nhndls; i != sz - 1; ++i)
            hndls[i].next = i + 1;
        hndls[sz - 1].next = -1;
        dill_unused = dill_nhndls;
        /* Adjust the array. */
        dill_hndls = hndls;
        dill_nhndls = sz;
    }
    /* Return first handle from the list of unused hadles. */
    int h = dill_unused;
    dill_unused = dill_hndls[h].next;
    dill_hndls[h].type = type;
    dill_hndls[h].data = data;
    dill_hndls[h].stop = stop;
    dill_hndls[h].done = 0;
    dill_hndls[h].next = -1;
    return h;
}

void *hndltype(int h) {
    if(dill_slow(h >= dill_nhndls || dill_hndls[h].next != -1)) {
        errno = EBADF; return NULL;}
    return dill_hndls[h].type;
}

void *hndldata(int h) {
    if(dill_slow(h >= dill_nhndls || dill_hndls[h].next != -1)) {
        errno = EBADF; return NULL;}
    return dill_hndls[h].data;
}

int hndldone(int h) {
    if(dill_slow(h >= dill_nhndls || dill_hndls[h].next != -1)) {
        errno = EBADF; return -1;}
    dill_hndls[h].done = 1;
    return 0;
}

int hndlclose(int h) {
    if(dill_slow(h >= dill_nhndls || dill_hndls[h].next != -1)) {
        errno = EBADF; return -1;}
    /* Return the handle to the list of unused handles. */
    dill_hndls[h].next = dill_unused;
    dill_unused = h;
    return 0;
}

