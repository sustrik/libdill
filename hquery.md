# hquery(3) manual page

## NAME

hquery - gets an opaque pointer associated with a handle and a type

## SYNOPSIS

```c
#include <libdill.h>
void *hquery(int h, const void *type);
```

## DESCRIPTION

Returns opaque pointer associated with the handle and the type. This function is a fundamental construct for building APIs on top of handles.

Argument `type` is not interpreted in any way. It is used only as a unique ID. An unique ID can be created, for example, like this:

```c
int foobar_placeholder = 0;
const void *foobar_type = &foobar_placeholder;
```

The return value has no specified semantics, it's an opaque pointer. Typical use case is to return pointer to a table of virtual functions. These virtual functions can be the used to access handle's functionality:

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

## RETURN VALUE

An opaque pointer in case of success. In case of failure, `NULL` is returned and `errno` is set to one of the following values.

## ERRORS

* `EDADF`: Invalid handle.
* `ENOTSUP`: Provided type parameter doesn't match any of the types supported by the handle.

## EXAMPLE

```c
struct foobar_vfs *vfs = hquery(h, foobar_type);
```

