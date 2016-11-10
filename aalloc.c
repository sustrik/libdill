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

#include "libdill.h"
#include "utils.h"

dill_unique_id(alloc_type);

void *aalloc(int h, size_t *sz) {
    struct alloc_vfs *a = hquery(h, alloc_type);
    if(dill_slow(!a)) return NULL;
    return a->alloc(a, sz);
}

int afree(int h, void *m) {
    struct alloc_vfs *a = hquery(h, alloc_type);
    if(dill_slow(!a)) return -1;
    return a->free(a, m);
}

ssize_t asize(int h) {
    struct alloc_vfs *a = hquery(h, alloc_type);
    if(dill_slow(!a)) return -1;
    return a->size(a);
}

int acaps(int h) {
    struct alloc_vfs *a = hquery(h, alloc_type);
    if(dill_slow(!a)) return -1;
    return a->caps(a);
}
