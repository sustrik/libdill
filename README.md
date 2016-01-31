# Simple coroutine library for C**

Coroutine is a standard C function with 'coroutine' modifier:

```
coroutine void foo(int arg1, int arg2, int arg3) {
    ...
}
```

To Start a coroutine use 'go' keyword. Expression return coroutine handle of
type 'coro'. Keep in mind that each coroutine handle has to be closed using
'gocancel' otherwise it will result in a memory leak.

`coro cr = go(foo(1, 2, 3));`

Coroutines are cooperatively scheduled. Every blocking function is a possible
switching point. If you are doing long computation without any blocking
functions you may want to yield CPU to different coroutines using 'yield'
function.

`int rc = yield();`

To cancel a coroutine, or several of them, use 'gocancel' function. This
function must be called for every coroutine, otherwise the resources used
by the coroutine will never be deallocated.

```
coro crs[2];
crs[0] = go(fx());
crs[1] = go(fx());
int rc = gocancel(crs, 2, now() + 1000);
```

The tihrd arugment is deadline. Coroutines being canceled will get grace period
to run until the deadline expires. Afterwards they will be canceled by force,
i.e. all blocking calls in the coroutine will return ECANCELED error.

All coroutines are canceled even if the coroutine doing cancellation is itself
canceled and gocancel() returns ECANCELED.

Semantics of channel are identical to semantics of channels in Go language.

To create a channel use 'channel' function:

`chan ch = channel(sizeof(int), 100);`

The line aboce creates a channel of int-sized elements able to hold 100 items.
To get an unbuffered channel set second parameter to zero.


Send a message to channel:

```
int val = 42;
int rc = chsend(ch, &val, sizeof(val));
```

Receive a message from channel:

```
int val;
int rc = chrecv(ch, &val, sizeof(val));
```

Mark a channel as non-functional:

```
int val = -1;
int rc = chdone(ch, &val, sizeof(val));
```

Once this function is called, all attempts to read from the channel will
return the specified value without blocking. All attems to send to the channel
will result in EPIPE error.

To duplicate a channel handle:

`chan ch2 = chdup(ch);`

The channel will be deallocated only after all the clones of the handle
are closed using 'chclose' function.

To close a channel:

`chclose(ch);`

To multiplex several channel operations use 'choose' function. Its semantics
closely mimic semantics of Golang's 'select' statement.

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
    /* val1 received from ch1 */
case 1:
    /* val2 sent to ch2 */
case -1:
    /* error */
}
```

Libdill doesn't use timeouts but rather deadlines. Deadline is a specific
point in time when the function should be canceled. To create a deadline
use 'now' function. The result it in milliseconds thus `now() + 1000` means
one second from now onward.

If deadline is reached functions fail and set errno to ETIMEDOUT.

Deadline -1 means "Never time out."

Deadline 0 means: "Perform the operation immediately. If not possile return
ETIMEDOUT."

To sleep until deadline use 'msleep' function:

`int rc = msleep(now() + 1000);`

Wait for a file descriptor:

`int rc = fdwait(fd, FDW_IN | FDW_OUT, now() + 1000);`

Libdill tries to minimise user/kernel mode switches by caching some information
about file descriptors. After closing a file descriptor you MUST call
`fdclean(fd)` function to clean the associated cache. Also, you MUST use
`mfork` function instead of standard `fork`.

A single pointer can be stored in coroutine-local storage.

To set it:

```
void *val = ...;
setcls(val);
```

To retrieve it:

```
void *val = cls();
```

Debugging:

```
gotrace(1); /* starts tracing */
goredump(); /* dumps info about current coroutines and channels */
```

Both functions can be invoked from the debugger.

The library is licensed under MIT/X11 license.
