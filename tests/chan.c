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
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>

#include "../libdill.h"

struct foo {
    int first;
    int second;
};

coroutine int sender(int ch, int doyield, int val) {
    if(doyield) {
        int rc = yield();
        assert(rc == 0);
    }
    int rc = chsend(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    return 0;
}

coroutine int receiver(int ch, int expected) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == expected);
    return 0;
}

coroutine int receiver2(int ch, int back) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    val = 0;
    rc = chsend(back, &val, sizeof(val), -1);
    assert(rc == 0);
    return 0;
}

coroutine int charsender(int ch, char val) {
    int rc = chsend(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    return 0;
}

coroutine int structsender(int ch, struct foo val) {
    int rc = chsend(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    return 0;
}

coroutine int sender2(int ch) {
    int val = 0;
    int rc = chsend(ch, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    return 0;
}

coroutine int receiver3(int ch) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    return 0;
}

coroutine int cancel(int ch) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == -1 && errno == ECANCELED);
    rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == -1 && errno == ECANCELED);
    return 0;
}

coroutine int sender3(int ch, int doyield) {
    if(doyield) {
        int rc = yield();
        assert(rc == 0);
    }
    int rc = chsend(ch, NULL, 0, -1);
    assert(rc == 0);
    return 0;
}

int main() {
    int val;

    /* Receiver waits for sender. */
    int ch1 = channel(sizeof(int), 0);
    assert(ch1 >= 0);
    int hndl1 = go(sender(ch1, 1, 333));
    assert(hndl1 >= 0);
    int rc = chrecv(ch1, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 333);
    hclose(ch1);
    rc = hwait(hndl1, NULL, -1);
    assert(rc == 0);

    /* Sender waits for receiver. */
    int ch2 = channel(sizeof(int), 0);
    assert(ch2 >= 0);
    int hndl2 = go(sender(ch2, 0, 444));
    assert(hndl2 >= 0);
    rc = chrecv(ch2, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 444);
    hclose(ch2);
    rc = hwait(hndl2, NULL, -1);
    assert(rc == 0);

    /* Test two simultaneous senders. */
    int ch3 = channel(sizeof(int), 0);
    assert(ch3 >= 0);
    int hndl3[2];
    hndl3[0] = go(sender(ch3, 0, 888));
    assert(hndl3[0] >= 0);
    hndl3[1] = go(sender(ch3, 0, 999));
    assert(hndl3[1] >= 0);
    rc = chrecv(ch3, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 888);
    rc = yield();
    assert(rc == 0);
    rc = chrecv(ch3, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 999);
    hclose(ch3);
    rc = hwait(hndl3[0], NULL, -1);
    assert(rc == 0);
    rc = hwait(hndl3[1], NULL, -1);
    assert(rc == 0);

    /* Test two simultaneous receivers. */
    int ch4 = channel(sizeof(int), 0);
    assert(ch4 >= 0);
    int hndl4[2];
    hndl4[0] = go(receiver(ch4, 333));
    assert(hndl4[0] >= 0);
    hndl4[1] = go(receiver(ch4, 444));
    assert(hndl4[1] >= 0);
    val = 333;
    rc = chsend(ch4, &val, sizeof(val), -1);
    assert(rc == 0);
    val = 444;
    rc = chsend(ch4, &val, sizeof(val), -1);
    assert(rc == 0);
    hclose(ch4);
    rc = hwait(hndl4[0], NULL, -1);
    assert(rc == 0);
    rc = hwait(hndl4[1], NULL, -1);
    assert(rc == 0);

    /* Test typed channels. */
    int hndl5[2];
    int ch5 = channel(sizeof(char), 0);
    assert(ch5 >= 0);
    hndl5[0] = go(charsender(ch5, 111));
    assert(hndl5[0] >= 0);
    char charval;
    rc = chrecv(ch5, &charval, sizeof(charval), -1);
    assert(rc == 0);
    assert(charval == 111);
    hclose(ch5);
    int ch6 = channel(sizeof(struct foo), 0);
    assert(ch6 >= 0);
    struct foo foo1 = {555, 222};
    hndl5[1] = go(structsender(ch6, foo1));
    assert(hndl5[1] >= 0);
    struct foo foo2;
    rc = chrecv(ch6, &foo2, sizeof(foo2), -1);
    assert(rc == 0);
    assert(foo2.first == 555 && foo2.second == 222);
    hclose(ch6);
    rc = hwait(hndl5[0], NULL, -1);
    assert(rc == 0);
    rc = hwait(hndl5[1], NULL, -1);
    assert(rc == 0);

    /* Test message buffering. */
    int ch7 = channel(sizeof(int), 2);
    assert(ch7 >= 0);
    val = 222;
    rc = chsend(ch7, &val, sizeof(val), -1);
    assert(rc == 0);
    val = 333;
    rc = chsend(ch7, &val, sizeof(val), -1);
    assert(rc == 0);
    rc = chrecv(ch7, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 222);
    rc = chrecv(ch7, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 333);
    val = 444;
    rc = chsend(ch7, &val, sizeof(val), -1);
    assert(rc == 0);
    rc = chrecv(ch7, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 444);
    val = 555;
    rc = chsend(ch7, &val, sizeof(val), -1);
    assert(rc == 0);
    val = 666;
    rc = chsend(ch7, &val, sizeof(val), -1);
    assert(rc == 0);
    rc = chrecv(ch7, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 555);
    rc = chrecv(ch7, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 666);
    hclose(ch7);

    /* Test simple chdone() scenarios. */
    int ch8 = channel(sizeof(int), 0);
    assert(ch8 >= 0);
    rc = chdone(ch8);
    assert(rc == 0);
    rc = chrecv(ch8, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    rc = chrecv(ch8, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    rc = chrecv(ch8, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    hclose(ch8);

    int ch10 = channel(sizeof(int), 10);
    assert(ch10 >= 0);
    val = 999;
    rc = chsend(ch10, &val, sizeof(val), -1);
    assert(rc == 0);
    rc = chdone(ch10);
    assert(rc == 0);
    rc = chrecv(ch10, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 999);
    rc = chrecv(ch10, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    rc = chrecv(ch10, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    hclose(ch10);

    int ch11 = channel(sizeof(int), 1);
    assert(ch11 >= 0);
    val = 222;
    rc = chsend(ch11, &val, sizeof(val), -1);
    assert(rc == 0);
    rc = chdone(ch11);
    assert(rc == 0);
    rc = chrecv(ch11, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 222);
    rc = chrecv(ch11, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    hclose(ch11);

    /* Test whether chdone() unblocks all receivers. */
    int ch12 = channel(sizeof(int), 0);
    assert(ch12 >= 0);
    int ch13 = channel(sizeof(int), 0);
    assert(ch13 >= 0);
    int hndl6[2];
    hndl6[0] = go(receiver2(ch12, ch13));
    assert(hndl6[0] >= 0);
    hndl6[1] = go(receiver2(ch12, ch13));
    assert(hndl6[1] >= 0);
    rc = chdone(ch12);
    assert(rc == 0);
    rc = chrecv(ch13, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 0);
    rc = chrecv(ch13, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 0);
    hclose(ch13);
    hclose(ch12);
    rc = hwait(hndl6[0], NULL, -1);
    assert(rc == 0);
    rc = hwait(hndl6[1], NULL, -1);
    assert(rc == 0);

    /* Test a combination of blocked sender and an item in the channel. */
    int ch14 = channel(sizeof(int), 1);
    assert(ch14 >= 0);
    val = 1;
    rc = chsend(ch14, &val, sizeof(val), -1);
    assert(rc == 0);
    int hndl7 = go(sender(ch14, 0, 2));
    assert(hndl7 >= 0);
    rc = chrecv(ch14, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 1);
    rc = chrecv(ch14, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 2);
    hclose(ch14);
    rc = hwait(hndl7, NULL, -1);
    assert(rc == 0);

    /* Test whether chdone() unblocks blocked senders. */
    int ch15 = channel(sizeof(int), 0);
    assert(ch15 >= 0);
    int hndl8[3];
    hndl8[0] = go(sender2(ch15));
    assert(hndl8[0] >= 0);
    hndl8[1] = go(sender2(ch15));
    assert(hndl8[1] >= 0);
    hndl8[2] = go(sender2(ch15));
    assert(hndl8[2] >= 0);
    rc = msleep(now() + 50);
    assert(rc == 0);
    rc = chdone(ch15);
    assert(rc == 0);
    hclose(ch15);
    rc = hwait(hndl8[0], NULL, -1);
    assert(rc == 0);
    rc = hwait(hndl8[1], NULL, -1);
    assert(rc == 0);
    rc = hwait(hndl8[2], NULL, -1);
    assert(rc == 0);

    /* Test whether hclose() unblocks blocked senders and receivers. */
    int ch16 = channel(sizeof(int), 0);
    assert(ch16 >= 0);
    int hndl9[2];
    hndl9[0] = go(receiver3(ch16));
    assert(hndl9[0] >= 0);
    hndl9[1] = go(receiver3(ch16));
    assert(hndl9[1] >= 0);
    rc = msleep(now() + 50);
    assert(rc == 0);
    hclose(ch16);
    rc = hwait(hndl9[0], NULL, -1);
    assert(rc == 0);
    rc = hwait(hndl9[1], NULL, -1);
    assert(rc == 0);

    /* Test cancelation. */
    int ch17 = channel(sizeof(int), 0);
    assert(ch17 >= 0);
    int hndl10 = go(cancel(ch17));
    assert(hndl10 >= 0);
    hclose(hndl10);
    hclose(ch17);

    /* Receiver waits for sender (zero-byte message). */
    int ch18 = channel(0, 0);
    assert(ch18 >= 0);
    int hndl11 = go(sender3(ch18, 1));
    assert(hndl11 >= 0);
    rc = chrecv(ch18, NULL, 0, -1);
    assert(rc == 0);
    hclose(ch18);
    rc = hwait(hndl11, NULL, -1);
    assert(rc == 0);

    /* Sender waits for receiver (zero-byte message). */
    int ch19 = channel(0, 0);
    assert(ch19 >= 0);
    int hndl12 = go(sender3(ch19, 0));
    assert(hndl12 >= 0);
    rc = chrecv(ch19, NULL, 0, -1);
    assert(rc == 0);
    hclose(ch19);
    rc = hwait(hndl12, NULL, -1);
    assert(rc == 0);

    /* Store multiple zero-byte messages in a buffered channel. */
    int ch20 = channel(0, 100);
    assert(ch20 >= 0);
    int i;
    for(i = 0; i != 10; ++i) {
        rc = chsend(ch20, NULL, 0, -1);
        assert(rc == 0);
    }
    for(i = 0; i != 10; ++i) {
        rc = chrecv(ch20, NULL, 0, -1);
        assert(rc == 0);
    }
    rc = chrecv(ch20, NULL, 0, now() + 10);
    assert(rc == -1 && errno == ETIMEDOUT);
    hclose(ch20);
    return 0;
}

