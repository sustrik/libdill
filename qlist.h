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

#ifndef DILL_QLIST_INCLUDED
#define DILL_QLIST_INCLUDED

#include <stddef.h>
#include "utils.h"

/* Singly-linked list. Works in LIFO manner, so it's actually a stack.
   However, not to confuse it with C language stack, let's call it a slist. */

struct dill_qlist {
    struct dill_qlist *next;
};

/* Initialise the list. */
static inline void dill_qlist_init(struct dill_qlist *self) {
    self->next = self;
}

/* True is the list has no items. */
static inline int dill_qlist_empty(struct dill_qlist *self) {
    return self->next == self;
}

/* Returns next item in the list. If 'it' is the list itself, it returns the
   first element in the list. If there are no more elements in the list
   returns pointer to the list itself. */
static inline struct dill_qlist *dill_qlist_next(struct dill_qlist *it) {
    return it->next;
}

/* Push the item to the beginning of the list. */
static inline void dill_qlist_push(struct dill_qlist *self,
      struct dill_qlist *item) {
    dill_assert(self);
    dill_assert(item);
    item->next = self->next;
    self->next = item;
}

/* Pop an item from the beginning of the list. */
static inline struct dill_qlist *dill_qlist_pop(struct dill_qlist *self) {
    dill_assert(self);
    struct dill_qlist *item = self->next;
    self->next = item->next;
    return item;
}

#endif

