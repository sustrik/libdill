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

#include <assert.h>

#include "../libdill.h"

static int type = 0;
static int data = 0;

void stop_fn1(int h) {
    assert(0);
}

void stop_fn2(int h) {
    int rc = handledone(h);
    assert(rc == 0);
}

coroutine void worker1(int h) {
    int rc = msleep(now() + 100);
    assert(rc == 0);
    rc = handledone(h);
    assert(rc == 0);
}

int main(void) {
    /* Handle is done before stop is called. */
    int h = handle(&type, &data, stop_fn1);
    assert(h >= 0);
    int rc = handledone(h);
    void *data = handledata(h);
    assert(!data);
    assert(rc == 0);
    rc = stop(&h, 1, -1);
    assert(rc == 0);

    /* Handle finishes synchronously when stop is called. */
    h = handle(&type, &data, stop_fn2);
    assert(h >= 0);
    rc = stop(&h, 1, 0);
    assert(rc == 0);

    /* Handle finishes asynchronously, no cancellation. */
    h = handle(&type, &data, stop_fn1);
    assert(h >= 0);
    int w = go(worker1(h));
    rc = stop(&h, 1, -1);
    assert(rc == 0);
    rc = stop(&w, 1, -1);
    assert(rc == 0);

    return 0;
}
