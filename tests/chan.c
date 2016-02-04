/*

  Copyright (c) 2015 Martin Sustrik

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

coroutine void sender(chan ch, int doyield, int val) {
    if(doyield) {
        int rc = yield();
        assert(rc == 0);
    }
    int rc = chsend(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    chclose(ch);
}

coroutine void receiver(chan ch, int expected) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == expected);
    chclose(ch);
}

coroutine void receiver2(chan ch, chan back) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    chclose(ch);
    val = 0;
    rc = chsend(back, &val, sizeof(val), -1);
    assert(rc == 0);
    chclose(back);
}

coroutine void charsender(chan ch, char val) {
    int rc = chsend(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    chclose(ch);
}

coroutine void structsender(chan ch, struct foo val) {
    int rc = chsend(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    chclose(ch);
}

coroutine void sender2(chan ch) {
    int val = 0;
    int rc = chsend(ch, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    chclose(ch);
}

coroutine void receiver3(chan ch) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
}

coroutine void cancel(chan ch) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == -1 && errno == ECANCELED);
    rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == -1 && errno == ECANCELED);
    chclose(ch);
}

int main() {
    int val;

    /* Receiver waits for sender. */
    chan ch1 = channel(sizeof(int), 0);
    coro cr1 = go(sender(chdup(ch1), 1, 333));
    int rc = chrecv(ch1, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 333);
    chclose(ch1);
    gocancel(&cr1, 1, -1);

    /* Sender waits for receiver. */
    chan ch2 = channel(sizeof(int), 0);
    coro cr2 = go(sender(chdup(ch2), 0, 444));
    rc = chrecv(ch2, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 444);
    chclose(ch2);
    gocancel(&cr2, 1, -1);

    /* Test two simultaneous senders. */
    chan ch3 = channel(sizeof(int), 0);
    coro cr3[2];
    cr3[0] = go(sender(chdup(ch3), 0, 888));
    cr3[1] = go(sender(chdup(ch3), 0, 999));
    rc = chrecv(ch3, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 888);
    rc = yield();
    assert(rc == 0);
    rc = chrecv(ch3, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 999);
    chclose(ch3);
    gocancel(cr3, 2, -1);

    /* Test two simultaneous receivers. */
    chan ch4 = channel(sizeof(int), 0);
    coro cr4[2];
    cr4[0] = go(receiver(chdup(ch4), 333));
    cr4[1] = go(receiver(chdup(ch4), 444));
    val = 333;
    rc = chsend(ch4, &val, sizeof(val), -1);
    assert(rc == 0);
    val = 444;
    rc = chsend(ch4, &val, sizeof(val), -1);
    assert(rc == 0);
    chclose(ch4);
    gocancel(cr4, 2, -1);

    /* Test typed channels. */
    coro cr5[2];
    chan ch5 = channel(sizeof(char), 0);
    cr5[0] = go(charsender(chdup(ch5), 111));
    char charval;
    rc = chrecv(ch5, &charval, sizeof(charval), -1);
    assert(rc == 0);
    assert(charval == 111);
    chclose(ch5);
    chan ch6 = channel(sizeof(struct foo), 0);
    struct foo foo1 = {555, 222};
    cr5[1] = go(structsender(chdup(ch6), foo1));
    struct foo foo2;
    rc = chrecv(ch6, &foo2, sizeof(foo2), -1);
    assert(rc == 0);
    assert(foo2.first == 555 && foo2.second == 222);
    chclose(ch6);
    gocancel(cr5, 2, -1);

    /* Test message buffering. */
    chan ch7 = channel(sizeof(int), 2);
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
    chclose(ch7);

    /* Test simple chdone() scenarios. */
    chan ch8 = channel(sizeof(int), 0);
    rc = chdone(ch8);
    assert(rc == 0);
    rc = chrecv(ch8, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    rc = chrecv(ch8, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    rc = chrecv(ch8, &val, sizeof(val), -1);
    assert(rc == -1 && errno == EPIPE);
    chclose(ch8);

    chan ch10 = channel(sizeof(int), 10);
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
    chclose(ch10);

    chan ch11 = channel(sizeof(int), 1);
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
    chclose(ch11);

    /* Test whether chdone() unblocks all receivers. */
    chan ch12 = channel(sizeof(int), 0);
    chan ch13 = channel(sizeof(int), 0);
    coro cr6[2];
    cr6[0] = go(receiver2(chdup(ch12), chdup(ch13)));
    cr6[1] = go(receiver2(chdup(ch12), chdup(ch13)));
    rc = chdone(ch12);
    assert(rc == 0);
    rc = chrecv(ch13, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 0);
    rc = chrecv(ch13, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 0);
    chclose(ch13);
    chclose(ch12);
    gocancel(cr6, 2, -1);

    /* Test a combination of blocked sender and an item in the channel. */
    chan ch14 = channel(sizeof(int), 1);
    val = 1;
    rc = chsend(ch14, &val, sizeof(val), -1);
    assert(rc == 0);
    coro cr7 = go(sender(chdup(ch14), 0, 2));
    rc = chrecv(ch14, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 1);
    rc = chrecv(ch14, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 2);
    chclose(ch14);
    gocancel(&cr7, 1, -1);

    /* Test whether chdone() unblocks blocked senders. */
    chan ch15 = channel(sizeof(int), 0);
    coro cr8[3];
    cr8[0] = go(sender2(chdup(ch15)));
    cr8[1] = go(sender2(chdup(ch15)));
    cr8[2] = go(sender2(chdup(ch15)));
    rc = msleep(now() + 50);
    assert(rc == 0);
    rc = chdone(ch15);
    assert(rc == 0);
    chclose(ch15);
    gocancel(cr8, 3, -1);

    /* Test whether chclose() unblocks blocked senders and receivers. */
    chan ch16 = channel(sizeof(int), 0);
    coro cr9[2];
    cr9[0] = go(receiver3(ch16));
    cr9[1] = go(receiver3(ch16));
    rc = msleep(now() + 50);
    assert(rc == 0);
    chclose(ch16);
    gocancel(cr9, 2, -1);

    /* Test cancelation. */
    chan ch17 = channel(sizeof(int), 0);
    coro cr10 = go(cancel(chdup(ch17)));
    gocancel(&cr10, 1, 0);
    chclose(ch17);

    return 0;
}

