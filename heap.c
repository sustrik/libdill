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

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

#include "heap.h"
#include "utils.h"

/* Returns index of the parent. */
#define dill_heap_parent(index) (((index) - 1) / 2)
/* Returns index of the left child. Increment by 1 to get index of the
   right child. */
#define dill_heap_child(index) (2 * (index) + 1)

void dill_heap_init(struct dill_heap *self) {
    self->items = NULL;
    self->count = 0;
    self->capacity = 0;
}

void dill_heap_term(struct dill_heap *self) {
    free(self->items);
}

int dill_heap_push(struct dill_heap *self, int64_t *val) {
    /* Grow the heap if needed. */
    if(dill_slow(self->count == self->capacity)) {
        size_t capacity = self->count ? self->count * 2 : 64;
        int64_t **items = realloc(self->items, capacity * sizeof(int64_t*));
        if(dill_slow(!items)) {errno = ENOMEM; return -1;}
        self->items = items;
        self->capacity = capacity;
    }
    /* Insert the new item into the heap. */
    size_t i = self->count;
    self->count++;
    self->items[i] = val;
    while(i) {
        size_t p = dill_heap_parent(i);
        int64_t *ival = self->items[i];
        int64_t *pval = self->items[p];
        if(*pval <= *ival) break;
        self->items[i] = pval;
        self->items[p] = ival;
        i = p;
    }
    return 0;
}

int64_t *dill_heap_pop(struct dill_heap *self) {
    if(dill_slow(!self->count)) return NULL;
    int64_t *result = self->items[0];
    self->count--;
    self->items[0] = self->items[self->count];
    size_t p = 0;
    while(1) {
        int64_t *pval = self->items[p];
        size_t ch = dill_heap_child(p);
        int64_t *chval = self->items[ch];
        /* If there are no children we are done. */
        if(ch >= self->count) break;
        /* If there are two children pick the lesser one. */
        if(ch + 1 < self->count && *self->items[ch + 1] < *chval) {
            ch++;
            chval = self->items[ch];
        }
        /* If child is greater that parent we are done. */
        if(*pval <= *chval) break;
        /* Swap the parent and the child. */
        self->items[ch] = pval;
        self->items[p] = chval;
        p = ch;
    }
    return result;
}

