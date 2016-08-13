# hdata(3) manual page

## NAME

hdata - gets the data pointer associated with a handle

## SYNOPSIS

```
#include <libdill.h>
void *hdata(int h, const void *type);
```

## DESCRIPTION

Returns opaque data pointer passed to `handle` funcion when the handle was created.

Second argument is the expected type of the handle. If the type of the supplied handle doesn't match the expected type the function fails.

## RETURN VALUE

The data pointer in case of success. In case of failure, `NULL` is returned and `errno` is set to one of the following values.

## ERRORS

* `EDADF`: Invalid handle.
* `ENOTSUP`: Provided type parameter doesn't match the typo of the handle.

## EXAMPLE

```
struct foo_data *data = hdata(h, foo_type);
```

