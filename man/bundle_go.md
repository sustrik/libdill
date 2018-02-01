# NAME

bundle_go - start a coroutine within a specified bundle

# SYNOPSIS

**#include &lt;libdill.h>**

**int bundle_go(int bndl, **_expression_**);**

# DESCRIPTION

Launches a coroutine that executes the function invocation passed as the _expression_ argument. The coroutine is executed concurrently, and its lifetime may exceed the lifetime of the caller coroutine. The return value of the coroutine, if any, is discarded and cannot be retrieved by the caller.

The coroutine will run inside of bundle specified by _bndl_ argument and it will be canceled when the bundle is closed.

Any function to be invoked with **bundle_go()** must be declared with the **coroutine** specifier.

*WARNING*: Coroutines will most likely work even without the **coroutine** specifier. However, they may fail in random non-deterministic ways, depending on the code in question and the particular combination of a compiler and optimization level. Additionally, arguments to a coroutine must not be function calls. If they are, the program may fail non-deterministically. If you need to pass a result of a computation to a coroutine, do the computation first, and then pass the result as an argument. Instead of:

```c
bundle_go(bndl, bar(foo(a)));
```

Do this:

```c
int a = foo();
bundle_go(bndl, bar(a));
```

# RETURN VALUE

Returns zero in the case of success. In the case of an error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ENOMEM**: Not enough memory to allocate the coroutine stack.

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

