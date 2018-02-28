# NAME

choose - performs one of multiple channel operations

# SYNOPSIS

```c
#include <libdill.h>

struct chclause {
    int op;
    int ch;
    void *val;
    size_t len;
};

int choose(struct chclause* clauses, int nclauses, int64_t deadline);
```

# DESCRIPTION

Accepts a list of channel operations. Performs one that can be done
first. If multiple operations can be done immediately, the one that
comes earlier in the array is executed.

**clauses**: Operations to choose from. See below.

**nclauses**: Number of clauses.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

The fields in **chclause** structure are as follows:

* **op**: Operation to perform. Either **CHSEND** or **CHRECV**.
* **ch**: The channel to perform the operation on.
* **val**: Buffer containing the value to send or receive.
* **len**: Size of the buffer.

# RETURN VALUE

In case of success the function returns index of the clause that caused the function to exit. In case of error it returns -1 and sets **errno** to one of the values below.

Even if an index is returned, **errno** may still be set to
an error value. The operation was successfull only if **errno**
is set to zero.

# ERRORS

* **ECANCELED**: Current coroutine was canceled.
* **EINVAL**: Invalid argument.
* **ETIMEDOUT**: Deadline was reached.

Additionally, if the function returns an index it can set **errno**
to one of the following values:

* **0**: Operation was completed successfully.
* **EBADF**: Invalid handle.
* **EINVAL**: Invalid parameter.
* **EMSGSIZE**: The peer expected a message with different size.
* **ENOTSUP**: Operation not supported. Presumably, the handle isn't a channel.
* **EPIPE**: Channel has been closed with **hdone**.

# EXAMPLE

```c
int val1 = 0;
int val2;
struct chclause clauses[] = {
    {CHSEND, ch, &val1, sizeof(val1)},
    {CHRECV, ch, &val2, sizeof(val2)}
};
int rc = choose(clauses, 2, now() + 1000);
if(rc == -1) {
    perror("Choose failed");
    exit(1);
}
if(rc == 0) {
    printf("Value %d sent.\n", val1);
}
if(rc == 1) {
    printf("Value %d received.\n", val2);
}
```
# SEE ALSO

chmake(3) chmake_mem(3) choose(3) chrecv(3) chsend(3) 
