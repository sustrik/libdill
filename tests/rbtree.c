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

#include "assert.h"
#include "../rbtree.c"

int main(void) {
    struct dill_rbtree tree;
    dill_rbtree_init(&tree);

    int rc = dill_rbtree_empty(&tree);
    assert(rc == 1);

    struct dill_rbtree_item items[10];
    int i;
    for(i = 0; i != 10; ++i) {
        dill_rbtree_insert(&tree, i, &items[i]);
    }

    i = 0;
    struct dill_rbtree_item *it = dill_rbtree_first(&tree);
    while(it) {
        assert(it == &items[i]);
        it = dill_rbtree_next(&tree, it);
        ++i;
    }
    assert(i == 10);

    dill_rbtree_erase(&tree, &items[0]);
    dill_rbtree_erase(&tree, &items[4]);
    dill_rbtree_erase(&tree, &items[5]);
    dill_rbtree_erase(&tree, &items[9]);

    rc = dill_rbtree_empty(&tree);
    assert(rc == 0);

    i = 0;
    it = dill_rbtree_first(&tree);
    while(it) {
        it = dill_rbtree_next(&tree, it);
        ++i;
    }
    assert(i == 6);

    return 0;
}

