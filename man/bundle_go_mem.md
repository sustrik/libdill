# NAME

bundle_go - start a coroutine within a specified bundle, on a user-supplied stack

# SYNOPSIS

**#include &lt;libdill.h>**

**int bundle_go_mem(int **_bndl_**, **_expression_**, void** \*_stk_, **size_t** _stklen_**);**

# DESCRIPTION

Launches a coroutine that executes the function invocation passed as the _expression_ argument. The coroutine is executed concurrently, and its lifetime may exceed the lifetime of the caller coroutine. The return value of the coroutine, if any, is discarded and cannot be retrieved by the caller.

The coroutine will run inside of bundle specified by _bndl_ argument and it will be canceled when the bundle is closed.

The buffer passed in the _stk_ argument will be used as the coroutine's stack. The length of the buffer should be specified in the _stklen_ argument.

Any function to be invoked with **bundle_go_mem()** must be declared with the **coroutine** specifier.

*WARNING*: Coroutines will most likely work even without the **coroutine** specifier. However, they may fail in random non-deterministic ways, depending on the code in question and the particular combination of a compiler and optimization level. Additionally, arguments to a coroutine must not be function calls. If they are, the program may fail non-deterministically. If you need to pass a result of a computation to a coroutine, do the computation first, and then pass the result as an argument. Instead of:

```c
bundle_go_mem(bndl, bar(foo(a)), stk, stklen);
```

Do this:

```c
int a = foo();
bundle_go_mem(bndl, hbar(a), stk, stklen);
```

# RETURN VALUE

Returns zero in the case of success. In the case of an error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ENOMEM**: Not enough memory to allocate the coroutine stack.

# EXAMPLE

```c
int b = bundle();
char stk1[16384];
char stk2[16384];
char stk3[16384];
bundle_go_mem(b, worker(), stk1, sizeof(stk1));
bundle_go_mem(b, worker(), stk2, sizeof(stk2));
bundle_go_mem(b, worker(), stk3, sizeof(stk3));
msleep(now() + 1000);
/* Cancel any workers that are still running. */
hclose(b);
```

