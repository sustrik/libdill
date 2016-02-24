# libdill: A simple coroutine library for C

## Define a coroutine

Coroutine is a standard C function returning `int` with `coroutine` modifier
applied:

```
coroutine int foo(int arg1, int arg2, int arg3) {
    printf("Hello world!\n");
    return 0;
}
```

## Launch a coroutine

To start a coroutine use `go` keyword. Expression returns a coroutine handle.

```
int h = go(foo(1, 2, 3));
```

Coroutines are cooperatively scheduled. Following functions are used as
scheduler switching points: `go()`, `yield()`, `hwait()`, `chsend()`,
`chrecv()`, `choose()`, `msleep()` and `fdwait()`.

If there's not enough memory available to allocate the stack for the new
coroutine `go()` returns `NULL` and sets `errno` to `ENOMEM`.

You can wait for coroutine termination via `hwait()` function. If successful
the `result` parameter of `hwait` function will be set to the value returned
by the coroutine.

To cancel a coroutine use `hclose()` function. Note that when coroutine handle
is closed, all blocking calls in the coroutine start failing with
`ECANCELED` error. `hclose()` will return coroutine's return value.

## Yield CPU to other coroutines

Occassionally, a coroutine has to perform a long calculation with no natural
coroutine switching points. Such calculation can block other coroutines from
executing for a long interval of time. To mitigate this problem call `yield()`
function once in a while.

```
int yield(void);
```

In case of success `yield()` returns 0. In case of error it returns -1 and
sets `errno` to one of the following values:

* `ECANCELED`: Current coroutine was canceled by its owner.

## Create a channel

Semantics of channel are identical to semantics of channels in Go language.

To create a channel use `channel()` function:

```
chan channel(size_t item_size, size_t buffer_size);
```

First argument is the size of one element in the channel. Second argument
is number of elements that can be stored in the channel. To get an unbuffered
channel set the second parameter to zero.

For example, this line creates a channel of int-sized elements able to hold
100 items:

```
chan ch = channel(sizeof(int), 100);
```

In case of success the function returns handle to the newly created channel.
In case of error it returns `NULL` and sets `errno` to one of the following
values:

* `EINVAL`: Invalid argument.
* `ENOMEM`: Not enough memory.

## Sending to a channel

Send a message to channel:

```
int val = 42;
int rc = chsend(ch, &val, sizeof(val), -1);
```

In case of success the function returns 0. In case of error it returns -1 and
sets `errno` to one of the following values:

* `EINVAL`: Invalid argument.
* `ETIMEDOUT`: The deadline expired before the message was sent.
* `EPIPE`: Communication was terminated via `chdone()` function.
* `ECANCELED`: The coroutine was canceled by its owner.
  
## Receiving from a channel

Receive a message from channel:

```
int val;
int rc = chrecv(ch, &val, sizeof(val), -1);
```

In case of success the function returns 0. In case of error it returns -1 and
sets `errno` to one of the following values:

* `EINVAL`: Invalid argument.
* `ETIMEDOUT`: The deadline expired before a message was received.
* `ECANCELED`: The coroutine was canceled by its owner.

## Terminating communication on a channel

Mark a channel as non-functional:

```
int chdone(chan ch);
```

Once this function is called all subsequent attemps to send to this channel
will result in `EPIPE` error. After all the items are received from the channel
all the subsequent attemps to receive will fail with `EPIPE` error.

In case of success the function returns 0. In case of error it returns -1 and
sets `errno` to one of the following values:

* `EINVAL`: Invalid argument.
* `EPIPE`: `chdone()` was already called on the channel.

## Duplicating a channel handle

To duplicate a channel handle:

```
chan chdup(chan ch);
```

The channel is deallocated only after all handles referring to it are closed.

## Closing a channel

To close a channel handle use `chclose()` function.

```
void chclose(chan ch);
```

The channel is deallocated only after all handles referring to it are closed.

## Working with multiple channels

To multiplex several channel operations use `choose()` function. Its semantics
closely mimic semantics of Go's `select` statement.

```
struct chclause {
    chan channel;
    int op;
    void *val;
    size_t len;
};

int choose(struct chclause *cls, int ncls, int64_t deadline);
```

Example:

```
int val1;
int val2 = 42;
struct chclause clauses[] = {
    {ch1, CHRECV, &val1, sizeof(val1)},
    {ch2, CHSEND, &val2, sizeof(val2)}
};
int rc = choose(clauses, 2, now() + 30000);
switch(rc) {
case 0:
    if(errno == EPIPE) {
        /* chdone was called on ch1 */
    }
    /* val1 received from ch1 */
case 1:
    if(errno == EPIPE) {
        /* chdone was called on ch2 */
    }
    /* val2 sent to ch2 */
case -1:
    /* error */
}
```

In case of error `choose()` returns -1 and sets errno to one of the following
values:

* `EINVAL`: Invalid argument.
* `ETIMEDOUT`: Deadline expired.
* `ECANCELED`: Current coroutine was canceled by its owner.

In case of success it returns index of the triggered clause and sets errno
to one of the following values:

* `0`: Operation succeeded.
* `EPIPE`: Communication on channel was terminated using `chdone()` function.

## Deadlines

Libdill uses deadlines rather than timeouts. Deadline is a specific
point in time when the function should be canceled. To create a deadline
use `now()` function. The result is in milliseconds, thus `now() + 1000` means
"one second from now onward".

If deadline is reached the function in question fails and sets `errno` to
`ETIMEDOUT`.

Deadline -1 means "Never time out."

Deadline 0 means "Perform the operation immediately. If not possile return
`ETIMEDOUT`." This is different from deadline of `now()` where there is no
guarantee that the operation will be attempted before timing out.

## Sleeping

To sleep until deadline expires use `msleep()` function:

`int rc = msleep(now() + 1000);`

The function returns 0 if it succeeds. In case of error it returns -1 and
sets errno to one of the following values:

* `ECANCELED`: Current coroutine was canceled by its owner.

## Waiting for file descriptors

Wait for a file descriptor:

`int rc = fdwait(fd, FDW_IN | FDW_OUT, now() + 1000);`

Libdill tries to minimise user/kernel mode transitions by caching some
information about file descriptors. After closing a file descriptor you MUST
call `fdclean(fd)` function to clean the associated cache. Also, you MUST use
`mfork()` function instead of standard `fork`.

On success, the function returns a combinatation of `FDW_IN`, `FDW_OUT` and
`FDW_ERR`. On error it returns -1 and sets `errno` to one of the following
values:

* `EINVAL`: Invalid argument.
* `ETIMEDOUT`: Deadline was reached.
* `ECANCELED`: Current coroutine was canceled by its owner.

## Coroutine-local storage

A single pointer can be stored in coroutine-local storage.

To set it use `setcls()` function:

```
void setcls(void *p);
```

To retrieve it use `cls()` function:

```
void *cls();
```

When new coroutine is launched the value of coroutine-local storage is
guaranteed to be `NULL`.

## Handles

Handles provide a mechanism to create asynchronous objects.

Handle is created by `handle()` function.

```
typedef void (*hclose_fn)(int h);
int handle(const void *type, void *data, hclose_fn close_fn);
```

`type` is a unique pointer identifying the type of the object. It can be
retrieved later on via `htype()` function.

`data` is an opaque pointer. It is not used by libdill. It can be retrieved
later on via `hdata()` function.

`close_fn` is a function that will be called when the handle is closed.
The implementation should try to stop the object as soon as possible
offering it no grace period. All blocking functions return ECANCELED if called
within the context of `close_fn`. `close_fn` should call `hdone()` once
the object is stopped. Once `hdone()` is called the object can be deallocated
and any associated resources can be freed.

On success, `handle()` function returns a newly allocated handle. On error,
-1 is returned and `errno` is set to one of the following values:

* `EINVAL`: Invalid argument.
* `ENOMEM`: Not enough memory to create the handle.

`htype()` function returns the type pointer as it was supplied to
`handle()` function when the handle was created.

```
const void *htype(int h);
```

In case of error the function returns `NULL` and sets `errno` to one of
the following values:

* `EBADF`: The specified handle does not exist.

`hdata()` function returns the data pointer as it was supplied to `handle()`
function when the handle was created.

```
void *hdata(int h);
```

In case of error the function returns `NULL` and sets `errno` to one of
the following values:

* `EBADF`: The specified handle does not exist.

`hdone()` should be called by the object when it is done doing its
work. Once the function is called, the object can be safely deallocated.
`result` parameter will be eventually passed to the owner of the handle
when it calls `hwait()` or `hclose()`.

```
int hdone(int h, int result);
```

In case of success 0 is returned. In case of error -1 is returned and `errno`
is set to one of the following values:

* `EBADF`: The specified handle does not exist.

`hwait()` is used to wait while handle finishes its work:

```
int hwait(int h, int *result, int64_t deadline);
```

In case of success `hwait()` returns 0. Handle is closed and cannot be used
any more. `result` parameter is set to handle's return value as it is supplied
by the implementation to `hdone()` function.

In case of error -1. Handle remains open. `errno` is set to one of the following
values:

* `EBADF`: The specified handle does not exist.
* `ETIMEDOUT`: The deadline was reached before the handle finished.
* `ECANCELED`: Current coroutine was closed by its owner.

`hclose()` is used to close the handle. The function immediately closes
the object it refers to and deallocates any associated resources.

```
int hclose(int h);
```

The function never fails. It returns handle's return value as passed by
the implementation to `hdone()` function.

## Debugging

Debugging:

```
gotrace(1); /* starts tracing */
goredump(); /* dumps info about current coroutines and channels */
```

If need be, both functions can be invoked directly from the debugger.

## License

The library is licensed under MIT/X11 license.

