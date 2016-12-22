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

#define rbtree_isnil(x) ((x) == &rbtree_nil)
#define rbtree_isroot(x) ((x)->up->up == &rbtree_nil)

static struct rbtree rbtree_nil = {
    0, &rbtree_nil, &rbtree_nil, &rbtree_nil};

void rbtree_init(struct rbtree *self) {
    self->red = 0;
    self->left = &rbtree_nil;
    self->right = &rbtree_nil;
    self->up = &rbtree_nil;
    self->val = 0;
}

int rbtree_empty(struct rbtree *self) {
    return rbtree_isnil(self->left);
}

static void rbtree_rleft(struct rbtree *it) {
    struct rbtree *x = it->right;
    it->right = x->left;
    if(!rbtree_isnil(it->left)) x->left->up = it;
    x->up = it->up;
    if(it == it->up->left) it->up->left = x;
    else it->up->right = x;
    x->left = it;
    it->up = x;
}

static void rbtree_rright(struct rbtree *it) {
    struct rbtree *x = it->left;
    it->left = x->right;
    if(!rbtree_isnil(x->right)) x->right->up = it;
    x->up = it->up;
    if(it == it->up->left) it->up->left = x;
    else it->up->right = x;
    x->right = it;
    it->up = x;
}

void rbtree_insert(struct rbtree *self, int64_t val, struct rbtree *x) {
    x->val = val;
    x->left=x->right = &rbtree_nil;
    struct rbtree *y = self;
    struct rbtree *z = self->left;
    while(!rbtree_isnil(z)) {
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
            struct rbtree *y = x->up->up->right;
            if(y->red) {
	              x->up->red = 0;
	              y->red = 0;
	              x->up->up->red = 1;
	              x = x->up->up;
            }
            else {
	              if(x == x->up->right) {
	                  x = x->up;
	                  rbtree_rleft(x);
	              }
	              x->up->red = 0;
	              x->up->up->red = 1;
	              rbtree_rright(x->up->up);
            }
        }
        else {
            struct rbtree *y = x->up->up->left;
            if (y->red) {
	              x->up->red = 0;
	              y->red = 0;
	              x->up->up->red = 1;
	              x = x->up->up;
            }
            else {
	              if(x == x->up->left) {
	                  x = x->up;
	                  rbtree_rright(x);
	              }
	              x->up->red = 0;
	              x->up->up->red = 1;
	              rbtree_rleft(x->up->up);
            }
        }
    }
    self->left->red = 0;
}

struct rbtree *rbtree_find(struct rbtree *self, int64_t val) {
    struct rbtree *x = self->left;
    if(rbtree_isnil(x)) return NULL;
    while(x->val != val) {
        if (x->val > val) x = x->left;
        else x = x->right;
        if(rbtree_isnil(x)) return NULL;
    }
    return x;
}

struct rbtree *rbtree_first(struct rbtree *self) {
    struct rbtree *x = self->left;
    if(rbtree_isnil(x)) return NULL;
    while(!rbtree_isnil(x->left)) x = x->left;
    return x;
}
  
struct rbtree* rbtree_next(struct rbtree *x) { 
    struct rbtree* y = x->right;
    if(!rbtree_isnil(y)) {
        while(!rbtree_isnil(y->left)) y=y->left;
        return y;
    }
    y=x->up;
    while(x == y->right) {
        x=y;
        y=y->up;
    }
    if(rbtree_isnil(y->up)) return NULL;
    return y;
}

static void rbtree_erase_fixup(struct rbtree *x) {
    while(!x->red && !rbtree_isroot(x)) {
        if(x == x->up->left) {
            struct rbtree *w = x->up->right;
            if(w->red) {
	              w->red = 0;
	              x->up->red = 1;
	              rbtree_rleft(x->up);
	              w = x->up->right;
            }
            if(w->right->red || w->left->red) {
	              if(!w->right->red) {
	                  w->left->red = 0;
	                  w->red = 1;
	                  rbtree_rright(w);
	                  w = x->up->right;
	              }
	              w->red = x->up->red;
	              x->up->red = 0;
	              w->right->red = 0;
	              rbtree_rleft(x->up);
	              return;
            }
            w->red = 1;
            x = x->up;
        }
        else {
            struct rbtree *w = x->up->left;
            if(w->red) {
	              w->red = 0;
	              x->up->red = 1;
	              rbtree_rright(x->up);
	              w = x->up->left;
            }
            if(w->right->red || w->left->red) {
	              if(!w->left->red) {
	                  w->right->red = 0;
	                  w->red = 1;
	                  rbtree_rleft(w);
	                  w = x->up->left;
	              }
	              w->red = x->up->red;
	              x->up->red = 0;
	              w->left->red = 0;
	              rbtree_rright(x->up);
	              return;
            }
            w->red = 1;
            x = x->up;
        }
    }
    x->red = 0;
}

void rbtree_erase(struct rbtree *z) {
    struct rbtree *y = (rbtree_isnil(z->left) || rbtree_isnil(z->right)) ?
        z : rbtree_next(z);
    struct rbtree *x = rbtree_isnil(y->left) ? y->right : y->left;
    x->up = y->up;
    if(rbtree_isroot(x)) {
        x->up->left = x;
    }
    else {
        if(y == y->up->left) y->up->left = x;
        else y->up->right = x;
    }
    if(y != z) {
        if(!y->red) rbtree_erase_fixup(x);
        y->left = z->left;
        y->right = z->right;
        y->up = z->up;
        y->red = z->red;
        z->left->up = z->right->up = y;
        if(z == z->up->left) z->up->left = y; 
        else z->up->right = y;
    }
    else {
        if(!y->red) rbtree_erase_fixup(x);
    }
}

