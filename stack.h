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

#ifndef DILL_STACK_INCLUDED
#define DILL_STACK_INCLUDED

#include <stddef.h>

struct dill_ctx_stack;
extern struct dill_ctx_stack dill_ctx_stack_main_data;

/* Initialises internal structures. */
int dill_initstack(void);

/* Release internal structures. */
void dill_termstack(void);

/* Allocates new stack. Returns pointer to the *top* of the stack.
   For now we assume that the stack grows downwards. */
void *dill_allocstack(size_t *stack_size);

/* Deallocates a stack. The argument is pointer to the top of the stack. */
void dill_freestack(void *stack);

#endif
