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

#ifndef TS_LIST_INCLUDED
#define TS_LIST_INCLUDED

/* Doubly-linked list. */

struct ts_list_item {
    struct ts_list_item *next;
    struct ts_list_item *prev;
};

struct ts_list {
    struct ts_list_item *first;
    struct ts_list_item *last;
};

/* Initialise the list. To statically initialise the list use = {0}. */
void ts_list_init(struct ts_list *self);

/* True is the list has no items. */
#define ts_list_empty(self) (!((self)->first))

/* Returns iterator to the first item in the list or NULL if
   the list is empty. */
#define ts_list_begin(self) ((self)->first)

/* Returns iterator to one past the item pointed to by 'it'. */
#define ts_list_next(it) ((it)->next)

/* Adds the item to the list before the item pointed to by 'it'.
   If 'it' is NULL the item is inserted to the end of the list. */
void ts_list_insert(struct ts_list *self, struct ts_list_item *item,
    struct ts_list_item *it);

/* Removes the item from the list and returns pointer to the next item in the
   list. Item must be part of the list. */
struct ts_list_item *ts_list_erase(struct ts_list *self,
    struct ts_list_item *item);

#endif

