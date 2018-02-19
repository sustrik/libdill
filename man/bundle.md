# NAME

bundle - create an empty coroutine bundle

# SYNOPSIS

**#include &lt;libdill.h>**

**int bundle(void);**

# DESCRIPTION

Coroutines are always running in bundles. Even a single coroutine created by **go()** gets its own bundle. A bundle is a lifetime control mechanism. When it is closed, all coroutines within the bundle are canceled.

This function creates an empty bundle. Coroutines can be added to the bundle using the **bundle_go()** and **bundle_go_mem()** functions.

If **hdone()** is called on a bundle, it waits until all coroutines exit. After calling **hdone()**, irrespective of whether it succeeds or times out, no further coroutines can be launched using the bundle.

When **hclose()** is called on the bundle, all the coroutines contained in the bundle will be canceled. In other words, all the blocking functions within the coroutine will start failing with an _ECANCELED_ error. The **hclose()** function itself won't exit until all the coroutines in the bundle exit.

# RETURN VALUE

Returns a handle to the bundle. In the case of an error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ENOMEM**: Not enough memory to allocate a new bundle.

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
