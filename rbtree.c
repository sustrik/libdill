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

#define dill_rbtree_isnil(self, x) ((x) == &(self)->nil)

void dill_rbtree_init(struct dill_rbtree *self) {
    self->root.red = 0;
    self->root.left = &self->nil;
    self->root.right = &self->nil;
    self->root.up = &self->nil;
    self->root.val = 0;
    self->nil.red = 0;
    self->nil.left = &self->nil;
    self->nil.right = &self->nil;
    self->nil.up = &self->nil;
    self->nil.val = 0;
}

int dill_rbtree_empty(struct dill_rbtree *self) {
    return dill_rbtree_isnil(self, self->root.left);
}

static void dill_rbtree_rleft(struct dill_rbtree *self,
      struct dill_rbtree_item *it) {
    struct dill_rbtree_item *x = it->right;
    it->right = x->left;
    if(!dill_rbtree_isnil(self, it->left)) x->left->up = it;
    x->up = it->up;
    if(it == it->up->left) it->up->left = x;
    else it->up->right = x;
    x->left = it;
    it->up = x;
}

static void dill_rbtree_rright(struct dill_rbtree *self,
      struct dill_rbtree_item *it) {
    struct dill_rbtree_item *x = it->left;
    it->left = x->right;
    if(!dill_rbtree_isnil(self, x->right)) x->right->up = it;
    x->up = it->up;
    if(it == it->up->left) it->up->left = x;
    else it->up->right = x;
    x->right = it;
    it->up = x;
}

void dill_rbtree_insert(struct dill_rbtree *self, int64_t val,
      struct dill_rbtree_item *x) {
    x->val = val;
    x->left=x->right = &self->nil;
    struct dill_rbtree_item *y = &self->root;
    struct dill_rbtree_item *z = self->root.left;
    while(!dill_rbtree_isnil(self, z)) {
        y = z;
        if(z->val > x->val) z = z->left;
        else z = z->right;
    }
    x->up = y;
    if(y == &self->root || y->val > x->val) y->left = x;
    else y->right = x;
    x->red = 1;
    while(x->up->red) {
        if(x->up == x->up->up->left) {
            struct dill_rbtree_item *y = x->up->up->right;
            if(y->red) {
	              x->up->red = 0;
	              y->red = 0;
	              x->up->up->red = 1;
	              x = x->up->up;
            }
            else {
	              if(x == x->up->right) {
	                  x = x->up;
	                  dill_rbtree_rleft(self, x);
	              }
	              x->up->red = 0;
	              x->up->up->red = 1;
	              dill_rbtree_rright(self, x->up->up);
            }
        }
        else {
            struct dill_rbtree_item *y = x->up->up->left;
            if (y->red) {
	              x->up->red = 0;
	              y->red = 0;
	              x->up->up->red = 1;
	              x = x->up->up;
            }
            else {
	              if(x == x->up->left) {
	                  x = x->up;
	                  dill_rbtree_rright(self, x);
	              }
	              x->up->red = 0;
	              x->up->up->red = 1;
	              dill_rbtree_rleft(self, x->up->up);
            }
        }
    }
    self->root.left->red = 0;
}

struct dill_rbtree_item *dill_rbtree_find(struct dill_rbtree *self,
      int64_t val) {
    struct dill_rbtree_item *x = self->root.left;
    if(dill_rbtree_isnil(self, x)) return NULL;
    while(x->val != val) {
        if (x->val > val) x = x->left;
        else x = x->right;
        if(dill_rbtree_isnil(self, x)) return NULL;
    }
    return x;
}

struct dill_rbtree_item *dill_rbtree_first(struct dill_rbtree *self) {
    struct dill_rbtree_item *x = self->root.left;
    if(dill_rbtree_isnil(self, x)) return NULL;
    while(!dill_rbtree_isnil(self, x->left)) x = x->left;
    return x;
}
  
struct dill_rbtree_item *dill_rbtree_next(struct dill_rbtree *self,
      struct dill_rbtree_item *x) { 
    struct dill_rbtree_item *y = x->right;
    if(!dill_rbtree_isnil(self, y)) {
        while(!dill_rbtree_isnil(self, y->left)) y=y->left;
        return y;
    }
    y=x->up;
    while(x == y->right) {
        x=y;
        y=y->up;
    }
    if(dill_rbtree_isnil(self, y->up)) return NULL;
    return y;
}

static void dill_rbtree_erase_fixup(struct dill_rbtree *self,
      struct dill_rbtree_item *x) {
    struct dill_rbtree_item *root = self->root.left;
    while(!x->red && root != x) {
        if(x == x->up->left) {
            struct dill_rbtree_item *w = x->up->right;
            if(w->red) {
	              w->red = 0;
	              x->up->red = 1;
	              dill_rbtree_rleft(self, x->up);
	              w = x->up->right;
            }
            if(w->right->red || w->left->red) {
	              if(!w->right->red) {
	                  w->left->red = 0;
	                  w->red = 1;
	                  dill_rbtree_rright(self, w);
	                  w = x->up->right;
	              }
	              w->red = x->up->red;
	              x->up->red = 0;
	              w->right->red = 0;
	              dill_rbtree_rleft(self, x->up);
	              x = root;
            }
            else {
                w->red = 1;
                x = x->up;
            }
        }
        else {
            struct dill_rbtree_item *w = x->up->left;
            if(w->red) {
	              w->red = 0;
	              x->up->red = 1;
	              dill_rbtree_rright(self, x->up);
	              w = x->up->left;
            }
            if(w->right->red || w->left->red) {
	              if(!w->left->red) {
	                  w->right->red = 0;
	                  w->red = 1;
	                  dill_rbtree_rleft(self, w);
	                  w = x->up->left;
	              }
	              w->red = x->up->red;
	              x->up->red = 0;
	              w->left->red = 0;
	              dill_rbtree_rright(self, x->up);
	              x = root;
            }
            else {
                w->red = 1;
                x = x->up;
            }
        }
    }
    x->red = 0;
}

void dill_rbtree_erase(struct dill_rbtree *self, struct dill_rbtree_item *z) {
    struct dill_rbtree_item *root = &self->root;
    struct dill_rbtree_item *y = (dill_rbtree_isnil(self, z->left) ||
        dill_rbtree_isnil(self, z->right)) ? z : dill_rbtree_next(self, z);
    struct dill_rbtree_item *x = dill_rbtree_isnil(self, y->left) ?
        y->right : y->left;
    x->up = y->up;
    if(x->up == root) {
        root->left = x;
    }
    else {
        if(y == y->up->left) y->up->left = x;
        else y->up->right = x;
    }
    if(y != z) {
        if(!y->red) dill_rbtree_erase_fixup(self, x);
        y->left = z->left;
        y->right = z->right;
        y->up = z->up;
        y->red = z->red;
        z->left->up = z->right->up = y;
        if(z == z->up->left) z->up->left = y; 
        else z->up->right = y;
    }
    else {
        if(!y->red) dill_rbtree_erase_fixup(self, x);
    }

    z->up = NULL;
    z->left = NULL;
    z->right = NULL;
}

