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

#include <stddef.h>
#include "utils.h"

/* Singly-linked list. Having both push and push_back functions means that
   it can be used both as a queue and as a stack. */

struct dill_slist_item {
    struct dill_slist_item *next;
};

struct dill_slist {
    struct dill_slist_item *first;
    struct dill_slist_item *last;
};

/* After removing item from a list, next points here. */
extern struct dill_slist_item dill_slist_item_none;

#define DILL_SLIST_ITEM_INITIALISER {&dill_slist_item_none}

/* Initialise a list item. */
static inline void dill_slist_item_init(struct dill_slist_item *self) {
    self->next = &dill_slist_item_none;
}

/* Returns 1 if the item is part of a list. 0 otherwise. */
static inline int dill_slist_item_inlist(struct dill_slist_item *self) {
    return self->next == &dill_slist_item_none ? 0 : 1;
}

/* Initialise the list. To statically initialise the list use = {0}. */
static inline void dill_slist_init(struct dill_slist *self) {
    self->first = NULL;
    self->last = NULL;
}

/* True is the list has no items. */
#define dill_slist_empty(self) (!((self)->first))

/* Returns iterator to the first item in the list or NULL if
   the list is empty. */
#define dill_slist_begin(self) ((self)->first)

/* Returns iterator to one past the item pointed to by 'it'.
   If there are no more items returns NULL. */
#define dill_slist_next(it) ((it)->next)

/* Push the item to the beginning of the list. */
static inline void dill_slist_push(struct dill_slist *self, struct dill_slist_item *item) {
    dill_assert(!dill_slist_item_inlist(item));
    item->next = self->first;
    self->first = item;
    if(!self->last)
        self->last = item;
}

/* Push the item to the end of the list. */
static inline void dill_slist_push_back(struct dill_slist *self,
    struct dill_slist_item *item) {
    dill_assert(!dill_slist_item_inlist(item));
    item->next = NULL;
    if(!self->last)
        self->first = item;
    else
        self->last->next = item;
    self->last = item;
}

/* Pop an item from the beginning of the list. */
static inline struct dill_slist_item *dill_slist_pop(struct dill_slist *self) {
    if(!self->first)
        return NULL;
    struct dill_slist_item *it = self->first;
    self->first = self->first->next;
    if(!self->first)
        self->last = NULL;
    it->next = &dill_slist_item_none;
    return it;
}

#endif

