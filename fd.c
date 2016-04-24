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

#include <sys/param.h>
#include <sys/resource.h>

#include "fd.h"
#include "utils.h"

int dill_maxfds(void) {
    /* Return cached value if possible. */
    static int maxfds = -1;
    if(dill_fast(maxfds >= 0)) return maxfds;
    /* Get the max number of file descriptors. */
    struct rlimit rlim;
    int rc = getrlimit(RLIMIT_NOFILE, &rlim);
    dill_assert(rc == 0);
    maxfds = rlim.rlim_max;
#if defined BSD
    /* The above behaves weirdly on newer versions of OSX, ruturning limit
       of -1. Fix it by using OPEN_MAX instead. */
    if(maxfds < 0)
        maxfds = OPEN_MAX;
#endif
    return maxfds;
}

