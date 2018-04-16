/*

  Copyright (c) 2017 Martin Sustrik

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

#include <stdint.h>

#include "cr.h"
#include "pollset.h"
#include "utils.h"

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"

int dill_msleep(int64_t deadline) {
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Actual waiting. */
    struct dill_tmclause tmcl;
    dill_timer(&tmcl, 1, deadline);
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    return 0;
}

int dill_fdin(int fd, int64_t deadline) {
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Start waiting for the fd. */
    struct dill_fdclause fdcl;
    rc = dill_pollset_in(&fdcl, 1, fd);
    if(dill_slow(rc < 0)) return -1;
    /* Optionally, start waiting for a timer. */
    struct dill_tmclause tmcl;
    dill_timer(&tmcl, 2, deadline);
    /* Block. */
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == 2)) {errno = ETIMEDOUT; return -1;}
    return 0;
}

int dill_fdout(int fd, int64_t deadline) {
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Start waiting for the fd. */
    struct dill_fdclause fdcl;
    rc = dill_pollset_out(&fdcl, 1, fd);
    if(dill_slow(rc < 0)) return -1;
    /* Optionally, start waiting for a timer. */
    struct dill_tmclause tmcl;
    dill_timer(&tmcl, 2, deadline);
    /* Block. */
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == 2)) {errno = ETIMEDOUT; return -1;}
    return 0;
}

int dill_fdclean(int fd) {
    return dill_pollset_clean(fd);
}

