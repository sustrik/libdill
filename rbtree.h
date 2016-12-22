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

#ifndef DILL_RBTREE_INCLUDED
#define DILL_RBTREE_INCLUDED

#include <stdint.h>

struct rbtree {
  int red;
  struct rbtree *left;
  struct rbtree *right;
  struct rbtree *up;
  int64_t val;
};

void rbtree_init(struct rbtree *self);

int rbtree_empty(struct rbtree *self);

void rbtree_insert(struct rbtree *self, int64_t val, struct rbtree *item);

void rbtree_erase(struct rbtree *item);

struct rbtree *rbtree_find(struct rbtree *self, int64_t val);

struct rbtree *rbtree_first(struct rbtree *self);

struct rbtree *rbtree_next(struct rbtree *it);

#endif
