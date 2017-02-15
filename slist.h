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

#ifndef DILL_SLIST_INCLUDED
#define DILL_SLIST_INCLUDED

#include "utils.h"

/* A Singly-linked list that's last-in-first-out, so it's actually a stack.
   To prevent confusion with C's call stack, we'll call it slist. */

struct dill_slist {
    struct dill_slist *next;
};

/* Initialize the list. */
static inline void dill_slist_init(struct dill_slist *self) {
    self->next = self;
}

/* True if the list has no items. */
static inline int dill_slist_empty(struct dill_slist *self) {
    return self->next == self;
}

/* Returns the next item in the list. If 'it' is the list itself, it returns the
   first element in the list. If there are no more elements in the list,
   returns a pointer to the list itself. */
#define dill_slist_next(it) ((it)->next)

/* Push the item to the beginning of the list. */
static inline void dill_slist_push(struct dill_slist *self,
      struct dill_slist *item) {
    item->next = self->next;
    self->next = item;
}

/* Pop an item from the beginning of the list. */
static inline struct dill_slist *dill_slist_pop(struct dill_slist *self) {
    struct dill_slist *item = self->next;
    self->next = item->next;
    return item;
}

#endif

