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

#include <stddef.h>
#include "utils.h"

/* Doubly-linked list. */

struct dill_list_item {
    struct dill_list_item *next;
    struct dill_list_item *prev;
};

struct dill_list {
    struct dill_list_item *first;
    struct dill_list_item *last;
};

/* After removing item from a list, prev & next point here. */
extern struct dill_list_item dill_list_item_none;

/* Initialise a list item. */
static inline void dill_list_item_init(struct dill_list_item *self) {
    self->prev = &dill_list_item_none;
    self->next = &dill_list_item_none;
}

/* Returns 1 if the item is part of a list. 0 otherwise. */
static inline int dill_list_item_inlist(struct dill_list_item *self) {
    return self->prev == &dill_list_item_none ? 0 : 1;
}

/* Initialise the list. To statically initialise the list use = {0}. */
static inline void dill_list_init(struct dill_list *self) {
    self->first = NULL;
    self->last = NULL;
}

/* True is the list has no items. */
#define dill_list_empty(self) (!((self)->first))

/* Returns iterator to the first item in the list or NULL if
   the list is empty. */
#define dill_list_begin(self) ((self)->first)

/* Returns iterator to one past the item pointed to by 'it'. */
#define dill_list_next(it) ((it)->next)

/* Adds the item to the list before the item pointed to by 'it'.
   If 'it' is NULL the item is inserted to the end of the list. */
static inline void dill_list_insert(struct dill_list *self, struct dill_list_item *item,
    struct dill_list_item *it) {
    item->prev = it ? it->prev : self->last;
    item->next = it;
    if(item->prev)
        item->prev->next = item;
    if(item->next)
        item->next->prev = item;
    if(!self->first || self->first == it)
        self->first = item;
    if(!it)
        self->last = item;
}

/* Removes the item from the list and returns pointer to the next item in the
   list. Item must be part of the list. */
static inline struct dill_list_item *dill_list_erase(struct dill_list *self,
    struct dill_list_item *item) {
    if(item->prev)
        item->prev->next = item->next;
    else
        self->first = item->next;
    if(item->next)
        item->next->prev = item->prev;
    else
        self->last = item->prev;
    struct dill_list_item *next = item->next;
    item->prev = &dill_list_item_none;
    item->next = &dill_list_item_none;
    return next;
}

#endif

