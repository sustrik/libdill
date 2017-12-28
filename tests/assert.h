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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define errno_assert(x) \
    do {\
        if(!(x)) {\
            fprintf(stderr, "%s [%d] (%s:%d)\n", strerror(errno),\
                (int)errno, __FILE__, __LINE__);\
            fflush(stderr);\
            abort();\
        }\
    } while(0)

#define choose_assert(x, err) \
    do {\
        if(rc != (x) || errno != (err)) {\
            fprintf(stderr, "rc=%d; %s [%d] (%s:%d)\n", (int)rc, \
                strerror(errno), (int)errno, __FILE__, __LINE__);\
            fflush(stderr);\
            abort();\
        }\
    } while(0)

#if DILL_SLOW_TESTS
#define DILL_TIME_PRECISION 1000
#else
#define DILL_TIME_PRECISION 20
#endif

#define time_assert(x, expected) \
    do {\
        if((x) < ((expected) - DILL_TIME_PRECISION) || \
              (x) > ((expected) + DILL_TIME_PRECISION)) {\
            fprintf(stderr, "Expected duration %d, actual duration %d "\
                "(%s:%d)\n", (int)(expected), (int)(x), __FILE__, __LINE__);\
            fflush(stderr);\
            abort();\
        }\
    } while(0) 

