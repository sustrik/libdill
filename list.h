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

#ifndef DILL_LIST_INCLUDED
#define DILL_LIST_INCLUDED

#include "utils.h"

/* Doubly-linked list. */
struct dill_list {
    struct dill_list *next;
    struct dill_list *prev;
};

/* Initialize the list. */
static inline void dill_list_init(struct dill_list *self) {
    self->next = self;
    self->prev = self;
}

/* True if the list has no items except for the head. */
static inline int dill_list_empty(struct dill_list *self) {
    return self->next == self;
}

/* True if the list has only one item in addition to the head. */
static inline int dill_list_oneitem(struct dill_list *self) {
    return self->next != self && self->next == self->prev;
}

/* Returns an iterator to one past the item pointed to by 'it'. If 'it' is the
   list itself it returns the first item of the list. At the end of
   the list, it returns the list itself. */
#define dill_list_next(it) ((it)->next)

/* Adds the item to the list before the item pointed to by 'before'. If 'before'
   is the list itself the item is inserted to the end of the list. */
static inline void dill_list_insert(struct dill_list *item,
      struct dill_list *before) {
    item->next = before;
    item->prev = before->prev;
    before->prev->next = item;
    before->prev = item;
}

/* Removes the item from the list. */
static void dill_list_erase(struct dill_list *item) {
    item->prev->next = item->next;
    item->next->prev = item->prev;
}

#endif

