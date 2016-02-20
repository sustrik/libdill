# libdill: A simple coroutine library for C

## Define a coroutine

Coroutine is a standard C function with `coroutine` modifier applied:

```
coroutine void foo(int arg1, int arg2, int arg3) {
    printf("Hello world!\n");
}
```

## Launch a coroutine

To start a coroutine use `go` keyword. Expression returns a coroutine handle.

```
int h = go(foo(1, 2, 3));
```

Coroutines are cooperatively scheduled. Following functions are used as
scheduler switching points: `go()`, `yield()`, `stop()`, `chsend()`,
`chrecv()`, `choose()`, `msleep()` and `fdwait()`.

Keep in mind that each coroutine handle has to be closed using `stop()`
otherwise it will result in a memory leak.

If there's not enough memory available to allocate the stack for the new
coroutine `go()` returns `NULL` and sets `errno` to `ENOMEM`.

## Cancel a coroutine

To cancel a coroutine use `stop()` function. This function deallocates
resources owned by the coroutine, therefore it has to be called even for
coroutines that have already finished executing. Failure to do so results
in resource leak.

```
int stop(int *hndls, int nhndls, int64_t deadline);
```

The function cancels `nhndls` coroutines in the array pointed to be `hndls`.
Third argument is a deadline. For detailed information about deadlines check
"Deadlines" section of this manual.

Example:

```
int hndls[2];
hndls[0] = go(fx());
hndls[1] = go(fx());
int rc = stop(hndls, 2, now() + 1000);
```

Coroutines being canceled will get grace period to run until the deadline
expires. Afterwards they will be canceled by force, i.e. all blocking calls
in the coroutine will return `ECANCELED` error.

The exact algorithm for this function is as follows:

1. If all the coroutines are already finished it succeeds straight away.
2. If not so it waits until the deadline. If all coroutines finish during
   this grace period the function succeeds immediately afterwards.
3. Otherwise it "kills" the remaining coroutines. What that means is that
   all function calls inside the coroutine in question from that point on will
   fail with `ECANCELED` error.
4. Finally, `stop()` waits until all the "killed" coroutines exit.
   It itself will succeed immediately afterwards.

In case of success `stop()` returns 0. In case or error it returns -1 and
sets errno to one of the following values:

* `EINVAL`: Invalid arguments.
* `ECANCELED`: Current coroutine was canceled by its owner. Coroutines in
   `hndls` array are cleanly canceled even in the case of this error.

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

## Debugging

Debugging:

```
gotrace(1); /* starts tracing */
goredump(); /* dumps info about current coroutines and channels */
```

If need be, both functions can be invoked directly from the debugger.

## License

The library is licensed under MIT/X11 license.

