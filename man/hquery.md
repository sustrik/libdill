# NAME

hquery - gets an opaque pointer associated with a handle and a type

# SYNOPSIS

**#include &lt;libdill.h>**

**void \*hquery(int** _h_**, const void** \*_type_**);**

# DESCRIPTION

Returns an opaque pointer associated with the passed handle and type. This function is a fundamental construct for building APIs on top of handles.

The _type_ argument is not interpreted in any way. It is used only as a unique ID. A unique ID can be created, for instance, like this:

```c
int foobar_placeholder = 0;
const void *foobar_type = &foobar_placeholder;
```

The return value has no specified semantics. It is an opaque pointer.  One typical use case for it is to return a pointer to a table of function pointers.  These function pointers can then be used to access the handle's functionality:

```c
struct foobar_vfs {
    int (*foo)(struct foobar_vfs *vfs, int a, int b);
    void (*bar)(struct foobar_vfs *vfs, char *c);
};

int foo(int h, int a, int b) {
    struct foobar_vfs *vfs = hquery(h, foobar_type);
    if(!vfs) return -1;
    return vfs->foo(vfs, a, b);
}

int foo(int h, char *c) {
    struct foobar_vfs *vfs = hquery(h, foobar_type);
    if(!vfs) return -1;
    vfs->bar(vfs, c);
    return 0;
}
```

Pointers returned by **hquery** are meant to be cachable. In other words, if you call **hquery** on the same handle with the same type multiple times, the result should be the same.

# RETURN VALUE

Returns an opaque pointer on success. On error, **NULL** is returned and _errno_ is set to one of the following values.


# ERRORS

* **EDADF**: Invalid handle.
* **ENOTSUP**: Provided type parameter doesn't match any of the types supported by the handle.

# EXAMPLE

```c
struct foobar_vfs *vfs = hquery(h, foobar_type);
```

