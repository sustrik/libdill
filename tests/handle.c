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
    /* Check whether blocking functions are disabled inside stop functions. */
    int rc = yield();
    assert(rc == -1 && errno == ECANCELED);
    rc = msleep(now() + 100);
    assert(rc == -1 && errno == ECANCELED);
    rc = fdwait(0, FDW_IN, -1);
    assert(rc == -1 && errno == ECANCELED);
    int ch = channel(0, 0);
    assert(ch >= 0);
    rc = chsend(ch, NULL, 0, -1);
    assert(rc == -1 && errno == ECANCELED);
    rc = chrecv(ch, NULL, 0, -1);
    assert(rc == -1 && errno == ECANCELED);
    hclose(ch);
    rc = hdone(h, 0);
    assert(rc == 0);
}

coroutine int worker1(int h) {
    int rc = msleep(now() + 100);
    assert(rc == 0);
    rc = hdone(h, 0);
    assert(rc == 0);
    return 0;
}

int main(void) {
    /* Handle is done before stop is called. */
    int h = handle(&type, &data, stop_fn1);
    assert(h >= 0);
    void *dt = hdata(h, &type);
    assert(dt == &data);
    int rc = hdone(h, 0);
    dt = hdata(h, &type);
    assert(!dt);
    assert(rc == 0);
    rc = hwait(h, NULL, -1);
    assert(rc == 0);

    /* Handle finishes synchronously when stop is called. */
    h = handle(&type, &data, stop_fn2);
    assert(h >= 0);
    hclose(h);

    /* Handle finishes asynchronously, no cancellation. */
    h = handle(&type, &data, stop_fn1);
    assert(h >= 0);
    int w = go(worker1(h));
    rc = hwait(h, NULL, -1);
    assert(rc == 0);
    rc = hwait(w, NULL, -1);
    assert(rc == 0);

    return 0;
}
