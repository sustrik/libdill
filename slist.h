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

/* Singly-linked list. Having both push and push_back functions means that
   it can be used both as a queue and as a stack. */

struct dill_slist_item {
    struct dill_slist_item *next;
};

struct dill_slist {
    struct dill_slist_item *first;
    struct dill_slist_item *last;
};

/* Initialise a list item. */
void dill_slist_item_init(struct dill_slist_item *self);

/* Returns 1 if the item is part of a list. 0 otherwise. */
int dill_slist_item_inlist(struct dill_slist_item *self);

/* Initialise the list. To statically initialise the list use = {0}. */
void dill_slist_init(struct dill_slist *self);

/* True is the list has no items. */
#define dill_slist_empty(self) (!((self)->first))

/* Returns iterator to the first item in the list or NULL if
   the list is empty. */
#define dill_slist_begin(self) ((self)->first)

/* Returns iterator to one past the item pointed to by 'it'.
   If there are no more items returns NULL. */
#define dill_slist_next(it) ((it)->next)

/* Push the item to the beginning of the list. */
void dill_slist_push(struct dill_slist *self, struct dill_slist_item *item);

/* Push the item to the end of the list. */
void dill_slist_push_back(struct dill_slist *self,
    struct dill_slist_item *item);

/* Pop an item from the beginning of the list. */
struct dill_slist_item *dill_slist_pop(struct dill_slist *self);

#endif

