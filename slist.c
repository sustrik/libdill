/*

  Copyright (c) 2015 Martin Sustrik

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

#include <stddef.h>

#include "slist.h"
#include "utils.h"

/* After removing item from a list, next points here. */
struct dill_slist_item dill_slist_item_none = {NULL};

void dill_slist_item_init(struct dill_slist_item *self) {
    self->next = &dill_slist_item_none;
}

int dill_slist_item_inlist(struct dill_slist_item *self) {
    return self->next == &dill_slist_item_none ? 0 : 1;
}

void dill_slist_init(struct dill_slist *self) {
    self->first = NULL;
    self->last = NULL;
}

void dill_slist_push(struct dill_slist *self, struct dill_slist_item *item) {
    dill_assert(!dill_slist_item_inlist(item));
    item->next = self->first;
    self->first = item;
    if(!self->last)
        self->last = item;
}

void dill_slist_push_back(struct dill_slist *self,
      struct dill_slist_item *item) {
    dill_assert(!dill_slist_item_inlist(item));
    item->next = NULL;
    if(!self->last)
        self->first = item;
    else
        self->last->next = item;
    self->last = item;
}

struct dill_slist_item *dill_slist_pop(struct dill_slist *self) {
    if(!self->first)
        return NULL;
    struct dill_slist_item *it = self->first;
    self->first = self->first->next;
    if(!self->first)
        self->last = NULL;
    it->next = &dill_slist_item_none;
    return it;
}

