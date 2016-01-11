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
#include <stdio.h>

#include "../treestack.h"

coroutine void sender1(chan ch, int val) {
    int rc = chs(ch, &val, sizeof(val));
    assert(rc == 0);
    chclose(ch);
}

coroutine void sender2(chan ch, int val) {
    int rc = yield();
    assert(rc == 0);
    rc = chs(ch, &val, sizeof(val));
    assert(rc == 0);
    chclose(ch);
}

coroutine void sender3(chan ch, int val, int64_t deadline) {
    int rc = msleep(deadline);
    assert(rc == 0);
    rc = chs(ch, &val, sizeof(val));
    assert(rc == 0);
    chclose(ch);
}

coroutine void receiver1(chan ch, int expected) {
    int val;
    int rc = chr(ch, &val, sizeof(val));
    assert(rc == 0);
    assert(val == expected);
    chclose(ch);
}

coroutine void receiver2(chan ch, int expected) {
    int rc = yield();
    assert(rc == 0);
    int val;
    rc = chr(ch, &val, sizeof(val));
    assert(rc == 0);
    assert(val == expected);
    chclose(ch);
}

coroutine void choosesender(chan ch, int val) {
    struct chclause cl = {ch, CHOOSE_CHS, &val, sizeof(val)};
    int rc = choose(&cl, 1, -1);
    assert(rc == 0);
    chclose(ch);
}

coroutine void feeder(chan ch, int val) {
    while(1) {
        int rc = chs(ch, &val, sizeof(val));
        assert(rc == 0);
        rc = yield();
        assert(rc == 0);
    }
}

coroutine void feeder2(chan ch, int first, int second) {
    struct chclause cls[] = {
        {ch, CHOOSE_CHS, &first, sizeof(first)},
        {ch, CHOOSE_CHS, &second, sizeof(second)}
    };
    while(1) {
        int rc = choose(cls, 2, -1);
        assert(rc >= 0);
    }
}

coroutine void feeder3(chan ch, int val) {
    while(1) {
        int rc = msleep(10);
        assert(rc == 0);
        rc = chs(ch, &val, sizeof(val));
        assert(rc == 0);
    }
}

coroutine void feeder4(chan ch) {
    while(1) {
        int val1 = 1;
        int val2 = 2;
        int val3 = 3;
        struct chclause cls[] = {
            {ch, CHOOSE_CHS, &val1, sizeof(val1)},
            {ch, CHOOSE_CHS, &val2, sizeof(val2)},
            {ch, CHOOSE_CHS, &val3, sizeof(val3)}
        };
        int rc = choose(cls, 3, -1);
        assert(rc >= 0);
    }
}

struct large {
    char buf[1024];
};

int main() {
    int rc;
    int val;

    /* In this test we are also going to make sure that the debugging support
       doesn't crash the application. */
    gotrace(1);

    /* Non-blocking receiver case. */
    chan ch1 = chmake(sizeof(int), 0);
    go(sender1(chdup(ch1), 555));
    struct chclause cls1[] = {{ch1, CHOOSE_CHR, &val, sizeof(val)}};
    rc = choose(cls1, 1, -1);
    assert(rc == 0);
    assert(val == 555);
    chclose(ch1);

    /* Blocking receiver case. */
    chan ch2 = chmake(sizeof(int), 0);
    go(sender2(chdup(ch2), 666));
    struct chclause cls2[] = {{ch2, CHOOSE_CHR, &val, sizeof(val)}};
    rc = choose(cls2, 1, -1);
    assert(rc == 0);
    assert(val == 666);
    chclose(ch2);

    /* Non-blocking sender case. */
    chan ch3 = chmake(sizeof(int), 0);
    go(receiver1(chdup(ch3), 777));
    val = 777;
    struct chclause cls3[] = {{ch3, CHOOSE_CHS, &val, sizeof(val)}};
    rc = choose(cls3, 1, -1);
    assert(rc == 0);
    chclose(ch3);

    /* Blocking sender case. */
    chan ch4 = chmake(sizeof(int), 0);
    go(receiver2(chdup(ch4), 888));
    val = 888;
    struct chclause cls4[] = {{ch4, CHOOSE_CHS, &val, sizeof(val)}};
    rc = choose(cls4, 1, -1);
    assert(rc == 0);
    chclose(ch4);

    /* Check with two channels. */
    chan ch5 = chmake(sizeof(int), 0);
    chan ch6 = chmake(sizeof(int), 0);
    go(sender1(chdup(ch6), 555));
    struct chclause cls5[] = {
        {ch5, CHOOSE_CHR, &val, sizeof(val)},
        {ch6, CHOOSE_CHR, &val, sizeof(val)}
    };
    rc = choose(cls5, 2, -1);
    assert(rc == 1);
    assert(val == 555);
    go(sender2(chdup(ch5), 666));
    rc = choose(cls5, 2, -1);
    assert(rc == 0);
    assert(val == 666);
    chclose(ch5);
    chclose(ch6);

    /* Test whether selection of in channels is random. */
    chan ch7 = chmake(sizeof(int), 0);
    chan ch8 = chmake(sizeof(int), 0);
    go(feeder(chdup(ch7), 111));
    go(feeder(chdup(ch8), 222));
    int i;
    int first = 0;
    int second = 0;
    int third = 0;
    for(i = 0; i != 100; ++i) {
        struct chclause cls6[] = {
            {ch7, CHOOSE_CHR, &val, sizeof(val)},
            {ch8, CHOOSE_CHR, &val, sizeof(val)}
        };
        rc = choose(cls6, 2, -1);
        assert(rc == 0 || rc == 1);
        if(rc == 0) {
            assert(val == 111);
            ++first;
        }
        if(rc == 1) {
            assert(val == 222);
            ++second;
        }
        int rc = yield();
        assert(rc == 0);
    }
    assert(first > 1 && second > 1);
    chclose(ch7);
    chclose(ch8);

    /* Test 'otherwise' clause. */
    chan ch9 = chmake(sizeof(int), 0);
    struct chclause cls7[] = {{ch9, CHOOSE_CHR, &val, sizeof(val)}};
    rc = choose(cls7, 1, 0);
    assert(rc == -1 && errno == ETIMEDOUT);
    chclose(ch9);
    rc = choose(NULL, 0, 0);
    assert(rc == -1 && errno == ETIMEDOUT);

    /* Test two simultaneous senders vs. choose statement. */
    chan ch10 = chmake(sizeof(int), 0);
    go(sender1(chdup(ch10), 888));
    go(sender1(chdup(ch10), 999));
    val = 0;
    struct chclause cls8[] = {{ch10, CHOOSE_CHR, &val, sizeof(val)}};
    rc = choose(cls8, 1, -1);
    assert(rc == 0);
    assert(val == 888);
    val = 0;
    rc = choose(cls8, 1, -1);
    assert(rc == 0);
    assert(val == 999);
    chclose(ch10);

    /* Test two simultaneous receivers vs. choose statement. */
    chan ch11 = chmake(sizeof(int), 0);
    go(receiver1(chdup(ch11), 333));
    go(receiver1(chdup(ch11), 444));
    val = 333;
    struct chclause cls9[] = {{ch11, CHOOSE_CHS, &val, sizeof(val)}};
    rc = choose(cls9, 1, -1);
    assert(rc == 0);
    val = 444;
    rc = choose(cls9, 1, -1);
    assert(rc == 0);
    chclose(ch11);

    /* Choose vs. choose. */
    chan ch12 = chmake(sizeof(int), 0);
    go(choosesender(chdup(ch12), 111));
    struct chclause cls10[] = {{ch12, CHOOSE_CHR, &val, sizeof(val)}};
    rc = choose(cls10, 1, -1);
    assert(rc == 0);
    assert(val == 111);
    chclose(ch12);

    /* Choose vs. buffered channels. */
    chan ch13 = chmake(sizeof(int), 2);
    val = 999;
    struct chclause cls11[] = {{ch13, CHOOSE_CHS, &val, sizeof(val)}};
    rc = choose(cls11, 1, -1);
    assert(rc == 0);
    struct chclause cls12[] = {{ch13, CHOOSE_CHR, &val, sizeof(val)}};
    rc = choose(cls12, 1, -1);
    assert(rc == 0);
    assert(val == 999);
    chclose(ch13);

    /* Test whether selection of out channels is random. */
    chan ch14 = chmake(sizeof(int), 0);
    go(feeder2(chdup(ch14), 666, 777));
    first = 0;
    second = 0;
    for(i = 0; i != 100; ++i) {
        int v;
        rc = chr(ch14, &v, sizeof(v));
        assert(rc == 0);
        if(v == 666)
            ++first;
        else if(v == 777)
            ++second;
        else
            assert(0);
    }
    assert(first > 1 && second > 1);
    chclose(ch14);

    /* Test whether allocating larger in buffer breaks previous in clause. */
    chan ch15 = chmake(sizeof(struct large), 1);
    chan ch16 = chmake(sizeof(int), 1);
    go(sender2(chdup(ch16), 1111));
    goredump();
    struct large lrg;
    struct chclause cls13[] = {
        {ch16, CHOOSE_CHR, &val, sizeof(val)},
        {ch15, CHOOSE_CHR, &lrg, sizeof(lrg)}
    };
    rc = choose(cls13, 2, -1);
    assert(rc == 0);
    assert(val == 1111);
    chclose(ch16);
    chclose(ch15);

    /* Test transferring a large object. */
    chan ch17 = chmake(sizeof(struct large), 1);
    struct large large = {{0}};
    rc = chs(ch17, &large, sizeof(large));
    assert(rc == 0);
    struct chclause cls14[] = {{ch17, CHOOSE_CHR, &lrg, sizeof(lrg)}};
    rc = choose(cls14, 1, -1);
    assert(rc == 0);
    chclose(ch17);

    /* Test that 'in' on done-with channel fires. */
    chan ch18 = chmake(sizeof(int), 0);
    val = 2222;
    rc = chdone(ch18, &val, sizeof(val));
    assert(rc == 0);
    struct chclause cls15[] = {{ch18, CHOOSE_CHR, &val, sizeof(val)}};
    rc = choose(cls15, 1, -1);
    assert(rc == 0);
    assert(val == 2222);
    chclose(ch18);

    /* Test whether selection of in channels is random when there's nothing
       immediately available to receive. */
    first = 0;
    second = 0;
    third = 0;
    chan ch19 = chmake(sizeof(int), 0);
    go(feeder3(chdup(ch19), 3333));
    for(i = 0; i != 100; ++i) {
        struct chclause cls16[] = {
            {ch19, CHOOSE_CHR, &val, sizeof(val)},
            {ch19, CHOOSE_CHR, &val, sizeof(val)},
            {ch19, CHOOSE_CHR, &val, sizeof(val)}
        };
        rc = choose(cls16, 3, -1);
        switch(rc) {
        case 0:
            ++first;
            break;
        case 1:
            ++second;
            break;
        case 2:
            ++third;
            break;
        default:
            assert(0);
        }
    }
    assert(first > 1 && second > 1 && third > 1);
    chclose(ch19);

    /* Test whether selection of out channels is random when sending
       cannot be performed immediately. */
    first = 0;
    second = 0;
    third = 0;
    chan ch20 = chmake(sizeof(int), 0);
    go(feeder4(chdup(ch20)));
    for(i = 0; i != 100; ++i) {
        int rc = msleep(10);
        assert(rc == 0);
        rc = chr(ch20, &val, sizeof(val));
        assert(rc == 0);
        switch(val) {
        case 1:
            ++first;
            break;
        case 2:
            ++second;
            break;
        case 3:
            ++third;
            break;
        default:
            assert(0);
        }
    }
    assert(first > 1 && second > 1 && third > 1);
    chclose(ch20);

    /* Test expiration of 'deadline' clause. */
    chan ch21 = chmake(sizeof(int), 0);
    int64_t start = now();
    struct chclause cls17[] = {{ch21, CHOOSE_CHR, &val, sizeof(val)}};
    rc = choose(cls17, 1, start + 50);
    assert(rc == -1 && errno == ETIMEDOUT);
    int64_t diff = now() - start;
    assert(diff > 30 && diff < 70);
    chclose(ch21);

    /* Test unexpired 'deadline' clause. */
    chan ch22 = chmake(sizeof(int), 0);
    start = now();
    go(sender3(chdup(ch22), 4444, start + 50));
    struct chclause cls18[] = {{ch22, CHOOSE_CHR, &val, sizeof(val)}};
    rc = choose(cls17, 1, start + 1000);
    assert(rc == 0);
    assert(val == 4444);
    diff = now() - start;
    assert(diff > 30 && diff < 70);
    chclose(ch22);

    return 0;
}

