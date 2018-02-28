# NAME

bundle_go - launches a coroutine within a bundle

# SYNOPSIS

```c
#include <libdill.h>

int bundle_go(int bndl, expression);
```

# DESCRIPTION

This construct launches a coroutine within the specified bundle.
For more information about bundles see **bundle**.

The coroutine gets a 1MB stack.
The stack is guarded by a non-writeable memory page. Therefore,
stack overflow will result in a **SEGFAULT** rather than overwriting
memory that doesn't belong to it.

**bndl**: Bundle to launch the coroutine in.

**expression**: Expression to evaluate as a coroutine.

The coroutine is executed concurrently, and its lifetime may exceed the
lifetime of the caller coroutine. The return value of the coroutine, if any,
is discarded and cannot be retrieved by the caller.

Any function to be invoked as a coroutine must be declared with the
**coroutine** specifier.

Use **hclose** to cancel the coroutine. When the coroutine is canceled
all the blocking calls within the coroutine will start failing with
**ECANCELED** error.

_WARNING_: Coroutines will most likely work even without the coroutine
specifier. However, they may fail in random non-deterministic ways,
depending on the code in question and the particular combination of compiler
and optimization level. Additionally, arguments to a coroutine must not be
function calls. If they are, the program may fail non-deterministically.
If you need to pass a result of a computation to a coroutine, do the
computation first, and then pass the result as an argument.  Instead of:

```
go(bar(foo(a)));
```

Do this:

```
int a = foo();
go(bar(a));
```

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine was canceled.
* **ENOMEM**: Not enough memory.
* **ENOTSUP**: The handle does not support this operation.

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

bundle(3) bundle_go_mem(3) bundle_mem(3) go(3) go_mem(3) yield(3) 
