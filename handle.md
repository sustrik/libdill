# handle(3) manual page

## NAME

handle - creates a handle

## SYNOPSIS

```c
#include <libdill.h>

struct hvfptrs {
    void (*close)(int h);
};

int handle(const void *type, void *data, const struct hvfptrs *vfptrs);
```

## DESCRIPTION

Handle is a user-space equivalent of a file descriptor. Coroutines, processes and channels are represented by handles.

Unlike with file descriptors though, you can create your own handle type. To do so use `handle` function.

First argument is an opaque pointer that uniquely represents the handle type. Second argument is an opaque pointer pointing to the handle instance. Third argument is a virtual function table of operations associated with the handle. At the moment the only supported operation is closing the handle.

When implementing the close operation keep in mind that it is not allowed to invoke blocking operations. Blocking operations invoked in the context of close operation will fail with `ECANCELED` error.

To close a handle use `hclose` function.

## RETURN VALUE

In case of success the function returns a newly allocated handle. In case of failure it returns -1 and sets `errno` to one of the values below.

## ERRORS

* `ECANCELED`: Current coroutine is being shut down.
* `EINVAL`: Invalid argument.
* `ENOMEM`: Not enough free memory to create the handle.

## EXAMPLE

```c
static const int foo_type_placeholder = 0;
static const void *foo_type = &foo_type_placeholder;
static void foo_close(int h);
static const struct hvfptrs foo_vfptrs = {foo_close};

struct foo_data {
    ...
};

int foo_create() {
    struct foo_data *data = malloc(sizeof(struct foo_data));
    ...
    return handle(foo_type, foodata, &foo_vfptrs);
}

static void foo_close(int h) {
    struct foo_data *data = hdata(h, foo_type);
    ...
    free(data);
}
```

