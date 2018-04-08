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

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>

#include "assert.h"
#include "../libdill.h"

struct foo {
    int first;
    int second;
};

coroutine void sender(int ch, int doyield, int val) {
    if(doyield) {
        int rc = yield();
        errno_assert(rc == 0);
    }
    int rc = chsend(ch, &val, sizeof(val), -1);
    errno_assert(rc == 0);
}

coroutine void receiver(int ch, int expected) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == expected);
}

coroutine void receiver2(int ch) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    val = 0;
    rc = chsend(ch, &val, sizeof(val), -1);
    errno_assert(rc == 0);
}

coroutine void charsender(int ch, char val) {
    int rc = chsend(ch, &val, sizeof(val), -1);
    errno_assert(rc == 0);
}

coroutine void structsender(int ch, struct foo val) {
    int rc = chsend(ch, &val, sizeof(val), -1);
    errno_assert(rc == 0);
}

coroutine void sender2(int ch) {
    int val = 0;
    int rc = chsend(ch, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
}

coroutine void receiver3(int ch) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
}

coroutine void cancel(int ch) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == ECANCELED);
    rc = chrecv(ch, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == ECANCELED);
}

coroutine void sender3(int ch, int doyield) {
    if(doyield) {
        int rc = yield();
        errno_assert(rc == 0);
    }
    int rc = chsend(ch, NULL, 0, -1);
    errno_assert(rc == 0);
}

int main() {
    int val;
    int rc;

    /* Receiver waits for sender. */
    int ch1[2];
    rc = chmake(ch1);
    errno_assert(rc == 0);
    assert(ch1[0] >= 0);
    assert(ch1[1] >= 0);
    int hndl1 = go(sender(ch1[0], 1, 333));
    errno_assert(hndl1 >= 0);
    rc = chrecv(ch1[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 333);
    rc = hclose(ch1[1]);
    errno_assert(rc == 0);
    rc = hclose(ch1[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl1);
    errno_assert(rc == 0);

    /* Sender waits for receiver. */
    int ch2[2];
    rc = chmake(ch2);
    errno_assert(rc == 0);
    int hndl2 = go(sender(ch2[0], 0, 444));
    errno_assert(hndl2 >= 0);
    rc = chrecv(ch2[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 444);
    rc = hclose(ch2[1]);
    errno_assert(rc == 0);
    rc = hclose(ch2[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl2);
    errno_assert(rc == 0);

    /* Test two simultaneous senders. */
    int ch3[2];
    rc = chmake(ch3);
    errno_assert(rc == 0);
    int hndl3[2];
    hndl3[0] = go(sender(ch3[0], 0, 888));
    errno_assert(hndl3[0] >= 0);
    hndl3[1] = go(sender(ch3[0], 0, 999));
    errno_assert(hndl3[1] >= 0);
    rc = chrecv(ch3[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 888);
    rc = yield();
    errno_assert(rc == 0);
    rc = chrecv(ch3[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 999);
    rc = hclose(ch3[1]);
    errno_assert(rc == 0);
    rc = hclose(ch3[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl3[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl3[1]);
    errno_assert(rc == 0);

    /* Test two simultaneous receivers. */
    int ch4[2];
    rc = chmake(ch4);
    errno_assert(rc == 0);
    int hndl4[2];
    hndl4[0] = go(receiver(ch4[0], 333));
    errno_assert(hndl4[0] >= 0);
    hndl4[1] = go(receiver(ch4[0], 444));
    errno_assert(hndl4[1] >= 0);
    val = 333;
    rc = chsend(ch4[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    val = 444;
    rc = chsend(ch4[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    rc = hclose(ch4[1]);
    errno_assert(rc == 0);
    rc = hclose(ch4[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl4[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl4[1]);
    errno_assert(rc == 0);

    /* Test simple chdone() scenario. */
    int ch8[2];
    rc = chmake(ch8);
    errno_assert(rc == 0);
    rc = chdone(ch8[0]);
    errno_assert(rc == 0);
    rc = chrecv(ch8[1], &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = chrecv(ch8[1], &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = chrecv(ch8[1], &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = hclose(ch8[1]);
    errno_assert(rc == 0);
    rc = hclose(ch8[0]);
    errno_assert(rc == 0);

    /* Test whether chdone() unblocks all receivers. */
    int ch12[2];
    rc = chmake(ch12);
    errno_assert(rc == 0);
    int hndl6[2];
    hndl6[0] = go(receiver2(ch12[0]));
    errno_assert(hndl6[0] >= 0);
    hndl6[1] = go(receiver2(ch12[0]));
    errno_assert(hndl6[1] >= 0);
    rc = chdone(ch12[1]);
    errno_assert(rc == 0);
    rc = chrecv(ch12[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 0);
    rc = chrecv(ch12[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 0);
    rc = hclose(ch12[1]);
    errno_assert(rc == 0);
    rc = hclose(ch12[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl6[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl6[1]);
    errno_assert(rc == 0);

    /* Test whether chdone() unblocks blocked senders. */
    int ch15[2];
    rc = chmake(ch15);
    errno_assert(rc == 0);
    int hndl8[3];
    hndl8[0] = go(sender2(ch15[0]));
    errno_assert(hndl8[0] >= 0);
    hndl8[1] = go(sender2(ch15[0]));
    errno_assert(hndl8[1] >= 0);
    hndl8[2] = go(sender2(ch15[0]));
    errno_assert(hndl8[2] >= 0);
    rc = msleep(now() + 50);
    errno_assert(rc == 0);
    rc = chdone(ch15[1]);
    errno_assert(rc == 0);
    rc = hclose(ch15[1]);
    errno_assert(rc == 0);
    rc = hclose(ch15[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl8[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl8[1]);
    errno_assert(rc == 0);
    rc = hclose(hndl8[2]);
    errno_assert(rc == 0);

    /* Test whether hclose() unblocks blocked senders and receivers. */
    int ch16[2];
    rc = chmake(ch16);
    errno_assert(rc == 0);
    int hndl9[2];
    hndl9[0] = go(receiver3(ch16[0]));
    errno_assert(hndl9[0] >= 0);
    hndl9[1] = go(receiver3(ch16[0]));
    errno_assert(hndl9[1] >= 0);
    rc = msleep(now() + 50);
    errno_assert(rc == 0);
    rc = hclose(ch16[1]);
    errno_assert(rc == 0);
    rc = hclose(ch16[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl9[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl9[1]);
    errno_assert(rc == 0);

    /* Test cancelation. */
    int ch17[2];
    rc = chmake(ch17);
    errno_assert(rc == 0);
    int hndl10 = go(cancel(ch17[0]));
    errno_assert(hndl10 >= 0);
    rc = hclose(hndl10);
    errno_assert(rc == 0);
    rc = hclose(ch17[1]);
    errno_assert(rc == 0);
    rc = hclose(ch17[0]);
    errno_assert(rc == 0);

    /* Receiver waits for sender (zero-byte message). */
    int ch18[2];
    rc = chmake(ch18);
    errno_assert(rc == 0);
    int hndl11 = go(sender3(ch18[0], 1));
    errno_assert(hndl11 >= 0);
    rc = chrecv(ch18[1], NULL, 0, -1);
    errno_assert(rc == 0);
    rc = hclose(ch18[1]);
    errno_assert(rc == 0);
    rc = hclose(ch18[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl11);
    errno_assert(rc == 0);

    /* Sender waits for receiver (zero-byte message). */
    int ch19[2];
    rc = chmake(ch19);
    errno_assert(rc == 0);
    int hndl12 = go(sender3(ch19[0], 0));
    errno_assert(hndl12 >= 0);
    rc = chrecv(ch19[1], NULL, 0, -1);
    errno_assert(rc == 0);
    rc = hclose(ch19[1]);
    errno_assert(rc == 0);
    rc = hclose(ch19[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl12);
    errno_assert(rc == 0);

    /* Channel with user-supplied storage. */
    struct chstorage mem;
    int ch20[2];
    rc = chmake_mem(&mem, ch20);
    errno_assert(rc == 0);
    rc = chrecv(ch20[0], NULL, 0, now() + 50);
    errno_assert(rc == -1 && errno == ETIMEDOUT);
    rc = hclose(ch20[1]);
    errno_assert(rc == 0);
    rc = hclose(ch20[0]);
    errno_assert(rc == 0);

    return 0;
}

