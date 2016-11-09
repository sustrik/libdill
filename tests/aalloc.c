/*

  Copyright (c) 2016 Martin Sustrik
  Copyright (c) 2016 Tai Chi Minh Ralph Eastwood

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
#include <stdio.h>
#include <unistd.h>

#include "../libdill.h"

coroutine void worker() {
}

int main() {
    int ac, am, ap, ao;
    size_t sz;
    void *p, *pa[8];
    int rc;
    size_t pgsz = sysconf(_SC_PAGE_SIZE);

    am = abasic(DILL_ALLOC_FLAGS_DEFAULT, 512);
    assert(am != -1);
    assert(asize(am) >= 512);
    assert(!(acaps(am) & DILL_ALLOC_CAPS_ZERO));
    assert(!(acaps(am) & DILL_ALLOC_CAPS_ALIGN));
    assert(!(acaps(am) & DILL_ALLOC_CAPS_BOOKKEEP));
    p = aalloc(am);
    hclose(am);

    ap = abasic(DILL_ALLOC_FLAGS_ALIGN, 1);
    assert(ap != -1);
    assert(asize(ap) >= pgsz);
    assert(!(acaps(ap) & DILL_ALLOC_CAPS_ZERO));
    assert(acaps(ap) & DILL_ALLOC_CAPS_ALIGN);
    assert(!(acaps(ap) & DILL_ALLOC_CAPS_BOOKKEEP));
    p = aalloc(ap);
    rc = afree(ap, p);
    hclose(ap);
#if HAVE_MPROTECT
    ap = abasic(DILL_ALLOC_FLAGS_GUARD, 512);
    assert(ap != -1);
    hclose(ap);
#else
    ap = abasic(DILL_ALLOC_FLAGS_GUARD, 512);
    assert(ap == -1);
#endif

    am = abasic(DILL_ALLOC_FLAGS_ZERO, 4096);
    assert(am != -1);
    assert(asize(am) >= 4096);
    assert(acaps(am) & DILL_ALLOC_CAPS_ZERO);
    assert(!(acaps(am) & DILL_ALLOC_CAPS_ALIGN));
    assert(!(acaps(am) & DILL_ALLOC_CAPS_BOOKKEEP));
    p = aalloc(am);
    while(sz)
        assert(((char *)p)[sz--] == 0);
    afree(am, p);
    hclose(am);
    ao = apool(DILL_ALLOC_FLAGS_DEFAULT, 8, 2);
    assert(ao != -1);
    assert(asize(ao) >= 8);
    assert(!(acaps(ao) & DILL_ALLOC_CAPS_ZERO));
    assert(acaps(ao) & DILL_ALLOC_CAPS_ALIGN);
    assert(acaps(ao) & DILL_ALLOC_CAPS_BOOKKEEP);
    p = aalloc(ao);
    assert(p != NULL);
    rc = afree(ao, p);
    assert(rc == 0);
    p = aalloc(ao);
    assert(p != NULL);
    rc = afree(ao, p);
    assert(rc == 0);
    p = aalloc(ao);
    assert(p != NULL);
    rc = afree(ao, p);
    assert(rc == 0);
    p = aalloc(ao);
    assert(p != NULL);
    rc = afree(ao, p);
    assert(rc == 0);
    hclose(ao);
    return 0;
}

