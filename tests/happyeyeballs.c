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

#include <stdio.h>

/* I hate mocking, but yeah, let's mock DNS and TCP to test this. */

#define dill_ipaddr_remotes mock_ipaddr_remotes
#define dill_tcp_connect mock_tcp_connect
#define dill_happyeyeballs_connect mock_happyeyeballs_connect

#include "assert.h"
#include "../happyeyeballs.c"

#define OP_REMOTES 1
#define OP_CONNECT 2
#define OP_END 3

struct step {
   int op;           /* Expected operation, see the codes above. */
   int mode;         /* Expected 'mode' argument to ipaddr_remotes. */
   const char *addr; /* Expected 'addr' argument to tcp_connect. */
   int64_t delay;    /* How much time should the operation take, in ms. */
   int cancel;       /* 1 if the operation is expected to be canceled. */
   int err;          /* Set errno to this value when function returs. */
   int res;          /* Return this value from the function. */
   int done;         /* Don't fill in. Used by mocks to mark finished steps. */
};

struct step *steps;
int next_step;

void start_test(struct step *ss, const char *name) {
    steps = ss;
    next_step = 0;
    int i = 0;
    while(steps[i].op != OP_END) {
        steps[i].done = 0;
        i++;
    } 
    printf("---------- %s\n", name);
}

void end_test(void) {
    assert(steps[next_step].op == OP_END);
    /* Make sure that there are no hanging steps. */
    int i = 0;
    while(steps[i].op != OP_END) {
        assert(steps[i].done);
        i++;
    } 
}

int mock_ipaddr_remotes(struct dill_ipaddr *addrs, int naddrs,
      const char *name, int port, int mode, int64_t deadline) {
    int this_step = next_step++;
    struct step *step = &steps[this_step];
    printf("step %d: ipaddr_remotes(\"%s\", %d, %s)\n", this_step, name, port,
        mode == DILL_IPADDR_IPV4 ? "IPADDR_IPV4" : "IPADDR_IPV6");
    assert(step->op == OP_REMOTES);
    assert(mode = step->mode);
    int rc = dill_msleep(dill_now() + step->delay);
    if(rc < 0 && errno == ECANCELED) {
        printf("step %d: canceled\n", this_step);
        assert(step->cancel);
        step->done = 1;
        return -1;
    }
    errno_assert(rc == 0);
    /* Generate some mock IP addresses. */
    const char *fmt = (mode == DILL_IPADDR_IPV4 ?
        "192.168.0.%d" : "2001::%d");
    int cnt = step->res >= 0 ? step->res : 0;
    if(naddrs < cnt) cnt = naddrs;
    int i;
    for(i = 0; i != cnt; i++) {
        char buf[DILL_IPADDR_MAXSTRLEN];
        snprintf(buf, sizeof(buf), fmt, i + 1);
        rc = dill_ipaddr_local(&addrs[i], buf, 80, 0);
        errno_assert(rc == 0);
    }
    assert(!step->cancel);
    errno = step->err;
    int res = step->res;
    step->done = 1;
    printf("step %d: end\n", this_step);
    return res;
}

int mock_tcp_connect(const struct dill_ipaddr *addr, int64_t deadline) {
    int this_step = next_step++;
    struct step *step = &steps[this_step];
    char buf[DILL_IPADDR_MAXSTRLEN];
    dill_ipaddr_str(addr, buf);
    printf("step %d: tcp_connect(%s)\n", this_step, buf);
    assert(step->op == OP_CONNECT);
    struct dill_ipaddr expected_addr;
    int rc = dill_ipaddr_local(&expected_addr, step->addr, 80, 0);
    errno_assert(rc == 0);
    assert(dill_ipaddr_equal(addr, &expected_addr, 0));
    rc = dill_msleep(dill_now() + step->delay);
    if(rc < 0 && errno == ECANCELED) {
        printf("step %d: canceled\n", this_step);
        assert(step->cancel);
        step->done = 1;
        return -1;
    }
    errno_assert(rc == 0);
    assert(!step->cancel);
    errno = step->err;
    int res = step->res;
    step->done = 1;
    printf("step %d: end\n", this_step);
    return res;
}

int main(void) {

    /* Simple IPv6 connection. */
    struct step test1[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 10, 0, 0, 1},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 10, 0, 0, 0},
        {OP_CONNECT, 0, "2001::1", 10, 0, 0, 5},
        {OP_END}
    };
    start_test(test1, "test1");
    int rc = dill_happyeyeballs_connect("www.example.org", 80, -1);
    assert(rc == 5);
    end_test();

    /* Simple IPv4 connection. */
    struct step test2[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 10, 0, 0, 0},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 10, 0, 0, 1},
        {OP_CONNECT, 0, "192.168.0.1", 10, 0, 0, 5},
        {OP_END}
    };
    start_test(test2, "test2");
    rc = dill_happyeyeballs_connect("www.example.org", 80, -1);
    assert(rc == 5);
    end_test();

    /* Check whether IPv4 DNS query is properly canceled. */
    struct step test3[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 10,  0, 0, 1},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 100, 1, 0, 0},
        {OP_CONNECT, 0, "2001::1", 10, 0, 0, 5},
        {OP_END}
    };
    start_test(test3, "test3");
    rc = dill_happyeyeballs_connect("www.example.org", 80, -1);
    assert(rc == 5);
    end_test();

    /* Check whether IPv6 DNS query is properly canceled. */
    struct step test4[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 1000, 1, 0, 0},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 10,   0, 0, 1},
        {OP_CONNECT, 0, "192.168.0.1", 10, 0, 0, 5},
        {OP_END}
    };
    start_test(test4, "test4");
    rc = dill_happyeyeballs_connect("www.example.org", 80, -1);
    assert(rc == 5);
    end_test();

    /* IPv6 address gets used first even if IPv4 address is resolved first. */
    struct step test5[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 30, 0, 0, 1},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 10, 0, 0, 1},
        {OP_CONNECT, 0, "2001::1", 10, 0, 0, 5},
        {OP_END}
    };
    start_test(test5, "test5");
    rc = dill_happyeyeballs_connect("www.example.org", 80, -1);
    assert(rc == 5);
    end_test();

    /* But IPv4 is used if IPv6 answer is delayed by more than 50ms. */
    struct step test6[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 100, 0, 0, 1},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 10,  0, 0, 1},
        {OP_CONNECT, 0, "192.168.0.1", 100, 0, 0, 5},
        {OP_END}
    };
    start_test(test6, "test6");
    rc = dill_happyeyeballs_connect("www.example.org", 80, -1);
    assert(rc == 5);
    end_test();

    /* But is IPv6 address comes first after the 50ms period we'll use it. */
    struct step test7[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 100, 0, 0, 1},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 200, 1, 0, 1},
        {OP_CONNECT, 0, "2001::1", 10, 0, 0, 5},
        {OP_END}
    };
    start_test(test7, "test7");
    rc = dill_happyeyeballs_connect("www.example.org", 80, -1);
    assert(rc == 5);
    end_test();

    /* Deadline expires while doing DNS queries. */
    struct step test8[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 1000, 1, 0, 0},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 1000, 1, 0, 0},
        {OP_END}
    };
    start_test(test8, "test8");
    rc = dill_happyeyeballs_connect("www.example.org", 80, dill_now() + 30);
    assert(rc == -1 && errno == ETIMEDOUT);
    end_test();

    /* Deadline expires while doing DNS queries (after initial 50ms period.) */
    struct step test9[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 1000, 1, 0, 0},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 1000, 1, 0, 0},
        {OP_END}
    };
    start_test(test9, "test9");
    rc = dill_happyeyeballs_connect("www.example.org", 80, dill_now() + 100);
    assert(rc == -1 && errno == ETIMEDOUT);
    end_test();

    /* IPv4 and IPv6 addresses should be alternated. Also check cancellation
       during connection attempt. */
    struct step test10[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 10, 0, 0, 2},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 20, 0, 0, 2},
        {OP_CONNECT, 0, "2001::1",     2000, 1, 0, 0},
        {OP_CONNECT, 0, "192.168.0.1", 2000, 1, 0, 0},
        {OP_CONNECT, 0, "2001::2",     2000, 1, 0, 0},
        {OP_END}
    };
    start_test(test10, "test10");
    rc = dill_happyeyeballs_connect("www.example.org", 80, dill_now() + 900);
    assert(rc == -1 && errno == ETIMEDOUT);
    end_test();

    /* If one family of addresses is delayed, start alternating once it
       arrives. */
    struct step test11[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 460, 0, 0, 3},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 10,  0, 0, 3},
        {OP_CONNECT, 0, "192.168.0.1", 2000, 1, 0, 0},
        {OP_CONNECT, 0, "192.168.0.2", 2000, 1, 0, 0},
        {OP_CONNECT, 0, "2001::1",     2000, 1, 0, 0},
        {OP_CONNECT, 0, "192.168.0.3", 50,   0, 0, 5},
        {OP_END}
    };
    start_test(test11, "test11");
    rc = dill_happyeyeballs_connect("www.example.org", 80, -1);
    assert(rc == 5);
    end_test();

    /* Same as 11 but the other way round. */
    struct step test12[] = {
        {OP_REMOTES, DILL_IPADDR_IPV6, NULL, 10, 0, 0, 3},
        {OP_REMOTES, DILL_IPADDR_IPV4, NULL, 400, 0, 0, 3},
        {OP_CONNECT, 0, "2001::1",     2000, 1, 0, 0},
        {OP_CONNECT, 0, "2001::2",     2000, 1, 0, 0},
        {OP_CONNECT, 0, "192.168.0.1", 2000, 1, 0, 0},
        {OP_CONNECT, 0, "2001::3",     50,   0, 0, 5},
        {OP_END}
    };
    start_test(test12, "test12");
    rc = dill_happyeyeballs_connect("www.example.org", 80, -1);
    assert(rc == 5);
    end_test();

    return 0;
}

