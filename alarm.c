/*

  Copyright (c) 2017 Tai Chi Minh Ralph Eastwood

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

#include <sys/time.h>
#include <signal.h>

#include "libdill.h"
#include "utils.h"

/* Poll count for tracking when we should trigger external events again. */
int dill_poll_count = 0;

static void dill_ctx_timer_handler(int sig, siginfo_t *si, void *uc) {
    dill_poll_count++;
}

int dill_alarm_init(void) {
    if(dill_fast(dill_poll_count != 0)) return 0;
    /* Call now() once to initialise. */
    now();
    /* Initialize the external event poll timer for 1 second intervals. */
    struct itimerval its;
    its.it_value.tv_sec = 1;
    its.it_value.tv_usec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_usec = its.it_value.tv_usec;
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = dill_ctx_timer_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) == -1) return 1;
    return setitimer(ITIMER_REAL, &its, NULL);
}
