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

#include <stddef.h>

#include "rbtree.h"

/* Credits: This implementation is heavily influenced by Emin Martinian's
   red-black tree implementation. */

#define dill_rbtree_isnil(x) ((x) == &dill_rbtree_nil)
#define dill_rbtree_isroot(x) ((x)->up->up == &dill_rbtree_nil)

static struct dill_rbtree dill_rbtree_nil = {
    0, &dill_rbtree_nil, &dill_rbtree_nil, &dill_rbtree_nil};

void dill_rbtree_init(struct dill_rbtree *self) {
    self->red = 0;
    self->left = &dill_rbtree_nil;
    self->right = &dill_rbtree_nil;
    self->up = &dill_rbtree_nil;
    self->val = 0;
}

int dill_rbtree_empty(struct dill_rbtree *self) {
    return dill_rbtree_isnil(self->left);
}

static void dill_rbtree_rleft(struct dill_rbtree *it) {
    struct dill_rbtree *x = it->right;
    it->right = x->left;
    if(!dill_rbtree_isnil(it->left)) x->left->up = it;
    x->up = it->up;
    if(it == it->up->left) it->up->left = x;
    else it->up->right = x;
    x->left = it;
    it->up = x;
}

static void dill_rbtree_rright(struct dill_rbtree *it) {
    struct dill_rbtree *x = it->left;
    it->left = x->right;
    if(!dill_rbtree_isnil(x->right)) x->right->up = it;
    x->up = it->up;
    if(it == it->up->left) it->up->left = x;
    else it->up->right = x;
    x->right = it;
    it->up = x;
}

void dill_rbtree_insert(struct dill_rbtree *self, int64_t val,
      struct dill_rbtree *x) {
    x->val = val;
    x->left=x->right = &dill_rbtree_nil;
    struct dill_rbtree *y = self;
    struct dill_rbtree *z = self->left;
    while(!dill_rbtree_isnil(z)) {
        y = z;
        if(z->val > x->val) z = z->left;
        else z = z->right;
    }
    x->up = y;
    if(y == self || y->val > x->val) y->left = x;
    else y->right = x;
    x->red = 1;
    while(x->up->red) {
        if(x->up == x->up->up->left) {
            struct dill_rbtree *y = x->up->up->right;
            if(y->red) {
	              x->up->red = 0;
	              y->red = 0;
	              x->up->up->red = 1;
	              x = x->up->up;
            }
            else {
	              if(x == x->up->right) {
	                  x = x->up;
	                  dill_rbtree_rleft(x);
	              }
	              x->up->red = 0;
	              x->up->up->red = 1;
	              dill_rbtree_rright(x->up->up);
            }
        }
        else {
            struct dill_rbtree *y = x->up->up->left;
            if (y->red) {
	              x->up->red = 0;
	              y->red = 0;
	              x->up->up->red = 1;
	              x = x->up->up;
            }
            else {
	              if(x == x->up->left) {
	                  x = x->up;
	                  dill_rbtree_rright(x);
	              }
	              x->up->red = 0;
	              x->up->up->red = 1;
	              dill_rbtree_rleft(x->up->up);
            }
        }
    }
    self->left->red = 0;
}

struct dill_rbtree *dill_rbtree_find(struct dill_rbtree *self, int64_t val) {
    struct dill_rbtree *x = self->left;
    if(dill_rbtree_isnil(x)) return NULL;
    while(x->val != val) {
        if (x->val > val) x = x->left;
        else x = x->right;
        if(dill_rbtree_isnil(x)) return NULL;
    }
    return x;
}

struct dill_rbtree *dill_rbtree_first(struct dill_rbtree *self) {
    struct dill_rbtree *x = self->left;
    if(dill_rbtree_isnil(x)) return NULL;
    while(!dill_rbtree_isnil(x->left)) x = x->left;
    return x;
}
  
struct dill_rbtree* dill_rbtree_next(struct dill_rbtree *x) { 
    struct dill_rbtree* y = x->right;
    if(!dill_rbtree_isnil(y)) {
        while(!dill_rbtree_isnil(y->left)) y=y->left;
        return y;
    }
    y=x->up;
    while(x == y->right) {
        x=y;
        y=y->up;
    }
    if(dill_rbtree_isnil(y->up)) return NULL;
    return y;
}

static void dill_rbtree_erase_fixup(struct dill_rbtree *x) {
    while(!x->red && !dill_rbtree_isroot(x)) {
        if(x == x->up->left) {
            struct dill_rbtree *w = x->up->right;
            if(w->red) {
	              w->red = 0;
	              x->up->red = 1;
	              dill_rbtree_rleft(x->up);
	              w = x->up->right;
            }
            if(w->right->red || w->left->red) {
	              if(!w->right->red) {
	                  w->left->red = 0;
	                  w->red = 1;
	                  dill_rbtree_rright(w);
	                  w = x->up->right;
	              }
	              w->red = x->up->red;
	              x->up->red = 0;
	              w->right->red = 0;
	              dill_rbtree_rleft(x->up);
	              return;
            }
            w->red = 1;
            x = x->up;
        }
        else {
            struct dill_rbtree *w = x->up->left;
            if(w->red) {
	              w->red = 0;
	              x->up->red = 1;
	              dill_rbtree_rright(x->up);
	              w = x->up->left;
            }
            if(w->right->red || w->left->red) {
	              if(!w->left->red) {
	                  w->right->red = 0;
	                  w->red = 1;
	                  dill_rbtree_rleft(w);
	                  w = x->up->left;
	              }
	              w->red = x->up->red;
	              x->up->red = 0;
	              w->left->red = 0;
	              dill_rbtree_rright(x->up);
	              return;
            }
            w->red = 1;
            x = x->up;
        }
    }
    x->red = 0;
}

void dill_rbtree_erase(struct dill_rbtree *z) {
    struct dill_rbtree *y = (dill_rbtree_isnil(z->left) ||
        dill_rbtree_isnil(z->right)) ? z : dill_rbtree_next(z);
    struct dill_rbtree *x = dill_rbtree_isnil(y->left) ? y->right : y->left;
    x->up = y->up;
    if(dill_rbtree_isroot(x)) {
        x->up->left = x;
    }
    else {
        if(y == y->up->left) y->up->left = x;
        else y->up->right = x;
    }
    if(y != z) {
        if(!y->red) dill_rbtree_erase_fixup(x);
        y->left = z->left;
        y->right = z->right;
        y->up = z->up;
        y->red = z->red;
        z->left->up = z->right->up = y;
        if(z == z->up->left) z->up->left = y; 
        else z->up->right = y;
    }
    else {
        if(!y->red) dill_rbtree_erase_fixup(x);
    }
}

