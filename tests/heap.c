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

#include "assert.h"
#include "../heap.c"

int main(void) {
    struct dill_heap h;
    dill_heap_init(&h);

    int64_t vals[10] = {1, 2, 3, 4, 15, 16, 33, 45, 46, 99};
    int i;
    for(i = 9; i >= 0; --i) {
        int rc = dill_heap_push (&h, &vals[i]);
        errno_assert(rc == 0);
    }
    for(i = 0; i != 10; ++i) {
        int64_t *p = dill_heap_pop(&h);
        assert(p == &vals[i]);
    }
    int64_t *p = dill_heap_pop(&h);
    assert(!p);

    dill_heap_term(&h);
    return 0;
}
