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

#include "slist.h"

/* A stack of unused coroutine stacks. This allows for extra-fast allocation
   of a new stack. The LIFO nature of this structure minimises cache misses.
   When the stack is cached its dill_slist_item is placed on its top rather
   then on the bottom. That way we minimise page misses. */
struct dill_ctx_stack {
    int count;
    struct dill_slist cache;
};

int dill_ctx_stack_init(struct dill_ctx_stack *ctx);
void dill_ctx_stack_term(struct dill_ctx_stack *ctx);

/* Allocates new stack. Returns pointer to the *top* of the stack.
   For now we assume that the stack grows downwards. */
void *dill_allocstack(size_t *stack_size);

/* Deallocates a stack. The argument is pointer to the top of the stack. */
void dill_freestack(void *stack);

#endif
