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


/* This implementation is based on Emin Martinian's implementation of the algorithm:
   http://web.mit.edu/~emin/Desktop/ref_to_emin/www.old/source_code/red_black_tree/index.html */

#include <stddef.h>

#include "rbtree.h"

void dill_rbtree_init(struct dill_rbtree *self) {
    struct dill_rbtree_item *temp;
    temp = &self->nil;
    temp->up = temp->left = temp->right=temp;
    temp->red = 0;
    temp->val = 0;
    temp = &self->root;
    temp->up = temp->left=temp->right = &self->nil;
    temp->val = 0;
    temp->red = 0;
}

static void dill_rbtree_lrotate(struct dill_rbtree* self,
      struct dill_rbtree_item* x) {
    struct dill_rbtree_item *y;
    struct dill_rbtree_item *nil = &self->nil;
    y = x->right;
    x->right = y->left;
    if(y->left != nil) y->left->up=x;
    y->up = x->up;   
    if(x == x->up->left) {
        x->up->left = y;
    } else {
        x->up->right = y;
    }
    y->left = x;
    x->up = y;
}

static void dill_rbtree_rrotate(struct dill_rbtree *self,
      struct dill_rbtree_item *y) {
    struct dill_rbtree_item *x;
    struct dill_rbtree_item *nil = &self->nil;
    x = y->left;
    y->left = x->right;
    if(nil != x->right) x->right->up = y;
    x->up = y->up;
    if(y == y->up->left) {
        y->up->left = x;
    } else {
        y->up->right = x;
    }
    x->right = y;
    y->up = x;
}

static void dill_rbtree_insert_help(struct dill_rbtree *self,
      struct dill_rbtree_item *z) {
    struct dill_rbtree_item *x;
    struct dill_rbtree_item *y;
    struct dill_rbtree_item *nil = &self->nil;
    z->left = z->right = nil;
    y = &self->root;
    x = self->root.left;
    while(x != nil) {
        y=x;
        if(x->val > z->val) { 
            x = x->left;
        } else {
            x = x->right;
        }
    }
    z->up = y;
    if((y == &self->root) || (y->val > z->val)) {
        y->left = z;
    } else {
        y->right = z;
    }
}

void dill_rbtree_insert(struct dill_rbtree *tree, int64_t val,
      struct dill_rbtree_item *item) {
    struct dill_rbtree_item *y;
    struct dill_rbtree_item *x;
    x = item;
    x->val = val;
    dill_rbtree_insert_help(tree, x);
    x->red = 1;
    while(x->up->red) {
        if(x->up == x->up->up->left) {
            y = x->up->up->right;
            if(y->red) {
                x->up->red = 0;
                y->red = 0;
                x->up->up->red = 1;
                x = x->up->up;
            } else {
                if(x == x->up->right) {
                    x = x->up;
                    dill_rbtree_lrotate(tree, x);
                }
                x->up->red = 0;
                x->up->up->red = 1;
                dill_rbtree_rrotate(tree, x->up->up);
            }
        } else {
            y = x->up->up->left;
            if(y->red) {
                x->up->red = 0;
                y->red = 0;
                x->up->up->red = 1;
                x = x->up->up;
            } else {
                if(x == x->up->left) {
                    x = x->up;
                    dill_rbtree_rrotate(tree, x);
                }
                x->up->red = 0;
                x->up->up->red = 1;
                dill_rbtree_lrotate(tree, x->up->up);
            } 
        }
    }
    tree->root.left->red = 0;
}

int dill_rbtree_empty(struct dill_rbtree *self) {
    struct dill_rbtree_item* nil = &self->nil;
    return self->root.left == nil;
}

struct dill_rbtree_item *dill_rbtree_first(struct dill_rbtree *self) {
    struct dill_rbtree_item* nil = &self->nil;
    struct dill_rbtree_item *x = self->root.left;
    if(x == nil) return NULL;
    while(x->left != nil) x = x->left;
    return x;
}
  
static struct dill_rbtree_item *dill_rbtree_next_help(struct dill_rbtree *tree,
      struct dill_rbtree_item *x) { 
    struct dill_rbtree_item *y;
    struct dill_rbtree_item *nil = &tree->nil;
    struct dill_rbtree_item *root = &tree->root;
    if(nil != (y = x->right)) {
        while(y->left != nil) {
            y = y->left;
        }
        return y;
    } else {
        y = x->up;
        while(x == y->right) {
            x = y;
            y = y->up;
        }
        if(y == root) return nil;
        return y;
    }
}

struct dill_rbtree_item *dill_rbtree_next(struct dill_rbtree *tree,
      struct dill_rbtree_item *x) {
    struct dill_rbtree_item *it = dill_rbtree_next_help(tree, x);
    if(it == &tree->nil) return NULL;
    return it;
}

static void dill_rbtree_fixup(struct dill_rbtree *tree,
      struct dill_rbtree_item *x) {
    struct dill_rbtree_item *root = tree->root.left;
    struct dill_rbtree_item *w;
    while((!x->red) && (root != x)) {
        if (x == x->up->left) {
            w = x->up->right;
            if(w->red) {
                w->red = 0;
                x->up->red = 1;
                dill_rbtree_lrotate(tree, x->up);
                w = x->up->right;
            }
            if((!w->right->red) && (!w->left->red)) {
                w->red = 1;
                x = x->up;
            } else {
                if(!w->right->red) {
                    w->left->red = 0;
                    w->red = 1;
                    dill_rbtree_rrotate(tree, w);
                    w = x->up->right;
                }
                w->red = x->up->red;
                x->up->red = 0;
                w->right->red = 0;
                dill_rbtree_lrotate(tree, x->up);
                x = root;
            }
        } else { 
            w = x->up->left;
            if(w->red) {
                w->red = 0;
                x->up->red = 1;
                dill_rbtree_rrotate(tree, x->up);
                w = x->up->left;
            }
            if((!w->right->red) && (!w->left->red)) {
                w->red = 1;
                x = x->up;
            } else {
                if(!w->left->red) {
                    w->right->red = 0;
                    w->red = 1;
                    dill_rbtree_lrotate(tree, w);
                    w = x->up->left;
                }
                w->red = x->up->red;
                x->up->red = 0;
                w->left->red = 0;
                dill_rbtree_rrotate(tree, x->up);
                x = root;
            }
        }
    }
    x->red = 0;
}

void dill_rbtree_erase(struct dill_rbtree *tree, struct dill_rbtree_item *z) {
    struct dill_rbtree_item *y;
    struct dill_rbtree_item *x;
    struct dill_rbtree_item *nil=&tree->nil;
    struct dill_rbtree_item *root=&tree->root;
    y = ((z->left == nil) || (z->right == nil)) ? z :
        dill_rbtree_next_help(tree, z);
    x = (y->left == nil) ? y->right : y->left;
    if(root == (x->up = y->up)) {
        root->left = x;
    } else {
        if(y == y->up->left) {
            y->up->left = x;
        } else {
            y->up->right = x;
        }
    }
    if(y != z) {
        if(!(y->red)) dill_rbtree_fixup(tree, x);
        y->left = z->left;
        y->right = z->right;
        y->up = z->up;
        y->red = z->red;
        z->left->up = z->right->up=y;
        if(z == z->up->left) {
            z->up->left=y; 
        } else {
            z->up->right=y;
        } 
    } else {
        if(!(y->red)) dill_rbtree_fixup(tree, x);    
    }
}

