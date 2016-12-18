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
struct dill_list {
    struct dill_list *next;
    struct dill_list *prev;
};

/* Initialise the list. To statically initialise the list use = {0}. */
static inline void dill_list_init(struct dill_list *self) {
    self->next = NULL;
    self->prev = NULL;
}

/* True is the list has no items. */
#define dill_list_empty(self) (!((self)->next))

/* Returns iterator to the first item in the list or NULL if
   the list is empty. */
#define dill_list_begin(self) ((self)->next)

/* Returns iterator to one past the item pointed to by 'it'. */
#define dill_list_next(it) ((it)->next)

/* Adds the item to the list before the item pointed to by 'it'.
   If 'it' is NULL the item is inserted to the end of the list. */
static inline void dill_list_insert(struct dill_list *self,
    struct dill_list *item, struct dill_list *it) {
    item->prev = it ? it->prev : self->prev;
    item->next = it;
    if(item->prev)
        item->prev->next = item;
    if(item->next)
        item->next->prev = item;
    if(!self->next || self->next == it)
        self->next = item;
    if(!it)
        self->prev = item;
}

/* Removes the item from the list and returns pointer to the next item in the
   list. Item must be part of the list. */
static inline struct dill_list *dill_list_erase(struct dill_list *self,
    struct dill_list *item) {
    if(item->prev)
        item->prev->next = item->next;
    else
        self->next = item->next;
    if(item->next)
        item->next->prev = item->prev;
    else
        self->prev = item->prev;
    return item->next;
}

#endif

