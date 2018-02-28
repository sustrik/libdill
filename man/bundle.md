# NAME

bundle - create an empty coroutine bundle

# SYNOPSIS

```c
#include <libdill.h>

int bundle(void);
```

# DESCRIPTION

Coroutines are always running in bundles. Even a single coroutine
created by **go** gets its own bundle. A bundle is a lifetime
control mechanism. When it is closed, all coroutines within the
bundle are canceled.

This function creates an empty bundle. Coroutines can be added to
the bundle using the **bundle_go** and **bundle_go_mem** functions.

If **hdone** is called on a bundle, it waits until all coroutines
exit. After calling **hdone**, irrespective of whether it succeeds
or times out, no further coroutines can be launched using the
bundle.

When **hclose()** is called on the bundle, all the coroutines
contained in the bundle will be canceled. In other words, all the
blocking functions within the coroutine will start failing with an
**ECANCELED** error. The **hclose()** function itself won't exit
until all the coroutines in the bundle exit.

# RETURN VALUE

In case of success the function returns handle of the newly create coroutine bundle. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.

# EXAMPLE

```c
int b = bundle();
bundle_go(b, worker());
bundle_go(b, worker());
bundle_go(b, worker());
msleep(now() + 1000);
/* Cancel any workers that are still running. */
hclose(b);
```
# SEE ALSO

bundle(3) bundle_go(3) bundle_go_mem(3) bundle_mem(3) go(3) go_mem(3) yield(3) 
