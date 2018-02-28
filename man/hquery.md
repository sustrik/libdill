# NAME

hquery - gets an opaque pointer associated with a handle and a type

# SYNOPSIS

```c
#include <libdillimpl.h>

void* hquery(int h);
```

# DESCRIPTION

Returns an opaque pointer associated with the passed handle and
type.  This function is a fundamental construct for building APIs
on top of handles.

The type argument is not interpreted in any way. It is used only
as an unique ID.  A unique ID can be created, for instance, like
this:

```
int foobar_placeholder = 0;
const void *foobar_type = &foobar_placeholder;
```

The  return value has no specified semantics. It is an opaque
pointer.  One typical use case for it is to return a pointer to a
table of function pointers.  These function pointers can then be
used to access the handle's functionality (see the example).

Pointers returned by **hquery** are meant to be cachable. In other
words, if you call hquery on the same handle with the same type
multiple times, the result should be the same.

**h**: The handle.

# RETURN VALUE

In case of success the function returns opaque pointer. In case of error it returns NULL and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.

# EXAMPLE

```c
struct quux_vfs {
    int (*foo)(struct quux_vfs *vfs, int a, int b);
    void (*bar)(struct quux_vfs *vfs, char *c);
};

int quux_foo(int h, int a, int b) {
    struct foobar_vfs *vfs = hquery(h, foobar_type);
    if(!vfs) return -1;
    return vfs->foo(vfs, a, b);
}

void quux_bar(int h, char *c) {
    struct foobar_vfs *vfs = hquery(h, foobar_type);
    if(!vfs) return -1;
    vfs->bar(vfs, c);
}
```
# SEE ALSO

hclose(3) hdone(3) hdup(3) hmake(3) 
