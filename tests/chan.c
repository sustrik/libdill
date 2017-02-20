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

coroutine void receiver2(int ch, int back) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    val = 0;
    rc = chsend(back, &val, sizeof(val), -1);
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
    int ch1 = chmake(sizeof(int));
    errno_assert(ch1 >= 0);
    int hndl1 = go(sender(ch1, 1, 333));
    errno_assert(hndl1 >= 0);
    rc = hdone(hndl1);
    errno_assert(rc == -1 && errno == ENOTSUP);
    rc = chrecv(ch1, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 333);
    hclose(ch1);
    rc = hclose(hndl1);
    errno_assert(rc == 0);

    /* Sender waits for receiver. */
    int ch2 = chmake(sizeof(int));
    errno_assert(ch2 >= 0);
    int hndl2 = go(sender(ch2, 0, 444));
    errno_assert(hndl2 >= 0);
    rc = chrecv(ch2, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 444);
    hclose(ch2);
    rc = hclose(hndl2);
    errno_assert(rc == 0);

    /* Test two simultaneous senders. */
    int ch3 = chmake(sizeof(int));
    errno_assert(ch3 >= 0);
    int hndl3[2];
    hndl3[0] = go(sender(ch3, 0, 888));
    errno_assert(hndl3[0] >= 0);
    hndl3[1] = go(sender(ch3, 0, 999));
    errno_assert(hndl3[1] >= 0);
    rc = chrecv(ch3, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 888);
    rc = yield();
    errno_assert(rc == 0);
    rc = chrecv(ch3, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 999);
    hclose(ch3);
    rc = hclose(hndl3[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl3[1]);
    errno_assert(rc == 0);

    /* Test two simultaneous receivers. */
    int ch4 = chmake(sizeof(int));
    errno_assert(ch4 >= 0);
    int hndl4[2];
    hndl4[0] = go(receiver(ch4, 333));
    errno_assert(hndl4[0] >= 0);
    hndl4[1] = go(receiver(ch4, 444));
    errno_assert(hndl4[1] >= 0);
    val = 333;
    rc = chsend(ch4, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    val = 444;
    rc = chsend(ch4, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    hclose(ch4);
    rc = hclose(hndl4[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl4[1]);
    errno_assert(rc == 0);

    /* Test typed channels. */
    int hndl5[2];
    int ch5 = chmake(sizeof(char));
    errno_assert(ch5 >= 0);
    hndl5[0] = go(charsender(ch5, 111));
    errno_assert(hndl5[0] >= 0);
    char charval;
    rc = chrecv(ch5, &charval, sizeof(charval), -1);
    errno_assert(rc == 0);
    assert(charval == 111);
    hclose(ch5);
    int ch6 = chmake(sizeof(struct foo));
    errno_assert(ch6 >= 0);
    struct foo foo1 = {555, 222};
    hndl5[1] = go(structsender(ch6, foo1));
    errno_assert(hndl5[1] >= 0);
    struct foo foo2;
    rc = chrecv(ch6, &foo2, sizeof(foo2), -1);
    errno_assert(rc == 0);
    assert(foo2.first == 555 && foo2.second == 222);
    hclose(ch6);
    rc = hclose(hndl5[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl5[1]);
    errno_assert(rc == 0);

    /* Test simple hdone() scenarios. */
    int ch8 = chmake(sizeof(int));
    errno_assert(ch8 >= 0);
    rc = hdone(ch8);
    errno_assert(rc == 0);
    rc = chrecv(ch8, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = chrecv(ch8, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = chrecv(ch8, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    hclose(ch8);

    /* Test whether hdone() unblocks all receivers. */
    int ch12 = chmake(sizeof(int));
    errno_assert(ch12 >= 0);
    int ch13 = chmake(sizeof(int));
    errno_assert(ch13 >= 0);
    int hndl6[2];
    hndl6[0] = go(receiver2(ch12, ch13));
    errno_assert(hndl6[0] >= 0);
    hndl6[1] = go(receiver2(ch12, ch13));
    errno_assert(hndl6[1] >= 0);
    rc = hdone(ch12);
    errno_assert(rc == 0);
    rc = chrecv(ch13, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 0);
    rc = chrecv(ch13, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 0);
    hclose(ch13);
    hclose(ch12);
    rc = hclose(hndl6[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl6[1]);
    errno_assert(rc == 0);

    /* Test whether hdone() unblocks blocked senders. */
    int ch15 = chmake(sizeof(int));
    errno_assert(ch15 >= 0);
    int hndl8[3];
    hndl8[0] = go(sender2(ch15));
    errno_assert(hndl8[0] >= 0);
    hndl8[1] = go(sender2(ch15));
    errno_assert(hndl8[1] >= 0);
    hndl8[2] = go(sender2(ch15));
    errno_assert(hndl8[2] >= 0);
    rc = msleep(now() + 50);
    errno_assert(rc == 0);
    rc = hdone(ch15);
    errno_assert(rc == 0);
    hclose(ch15);
    rc = hclose(hndl8[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl8[1]);
    errno_assert(rc == 0);
    rc = hclose(hndl8[2]);
    errno_assert(rc == 0);

    /* Test whether hclose() unblocks blocked senders and receivers. */
    int ch16 = chmake(sizeof(int));
    errno_assert(ch16 >= 0);
    int hndl9[2];
    hndl9[0] = go(receiver3(ch16));
    errno_assert(hndl9[0] >= 0);
    hndl9[1] = go(receiver3(ch16));
    errno_assert(hndl9[1] >= 0);
    rc = msleep(now() + 50);
    errno_assert(rc == 0);
    hclose(ch16);
    rc = hclose(hndl9[0]);
    errno_assert(rc == 0);
    rc = hclose(hndl9[1]);
    errno_assert(rc == 0);

    /* Test cancelation. */
    int ch17 = chmake(sizeof(int));
    errno_assert(ch17 >= 0);
    int hndl10 = go(cancel(ch17));
    errno_assert(hndl10 >= 0);
    hclose(hndl10);
    hclose(ch17);

    /* Receiver waits for sender (zero-byte message). */
    int ch18 = chmake(0);
    errno_assert(ch18 >= 0);
    int hndl11 = go(sender3(ch18, 1));
    errno_assert(hndl11 >= 0);
    rc = chrecv(ch18, NULL, 0, -1);
    errno_assert(rc == 0);
    hclose(ch18);
    rc = hclose(hndl11);
    errno_assert(rc == 0);

    /* Sender waits for receiver (zero-byte message). */
    int ch19 = chmake(0);
    errno_assert(ch19 >= 0);
    int hndl12 = go(sender3(ch19, 0));
    errno_assert(hndl12 >= 0);
    rc = chrecv(ch19, NULL, 0, -1);
    errno_assert(rc == 0);
    hclose(ch19);
    rc = hclose(hndl12);
    errno_assert(rc == 0);

    /* Channel with user-supplied storage. */
    struct chmem mem;
    int ch20 = chmake_mem(0, &mem);
    errno_assert(ch20 >= 0);
    rc = chrecv(ch20, NULL, 0, now() + 50);
    errno_assert(rc == -1 && errno == ETIMEDOUT);
    rc = hclose(ch20);
    errno_assert(rc == 0);

    return 0;
}

