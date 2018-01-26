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
#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include "../libdill.h"

coroutine void worker1(void) {
    int val;
    int rc = chrecv(hctrl, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 333);
    val = 444;
    rc = chsend(hctrl, &val, sizeof(val), -1);
    errno_assert(rc == 0);
}

coroutine void worker2(void) {
    int val;
    int rc = chrecv(hctrl, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = hdone(hctrl, -1);
    errno_assert(rc == 0);
}

coroutine void worker3(void) {
    int rc = msleep(now() + 10);
    errno_assert(rc == 0);
}

int main(void) {
    /* Do a handshake with the child coroutine. */
    int hndl1 = go(worker1());
    errno_assert(hndl1 >= 0);
    int val = 333;
    int rc = chsend(hndl1, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    rc = chrecv(hndl1, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 444);
    rc = hclose(hndl1);
    errno_assert(rc == 0);

    /* Do a handshake doing hdone on the control channel. */
    int hndl2 = go(worker2());
    errno_assert(hndl2 >= 0);
    rc = hdone(hndl2, -1);
    errno_assert(rc == 0);
    rc = chrecv(hndl2, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = hclose(hndl2);
    errno_assert(rc == 0);

    /* Check whether control channel is done after the coroutine finishes. */
    int hndl3 = go(worker3());
    errno_assert(hndl3 >= 0);
    rc = chrecv(hndl3, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);    
    rc = hclose(hndl3);
    errno_assert(rc == 0);
    int hndl4 = go(worker3());
    errno_assert(hndl4 >= 0);
    rc = chsend(hndl4, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);    
    rc = hclose(hndl4);
    errno_assert(rc == 0);


    return 0;
}

