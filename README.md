**C-idiomatic API for libmill. An experiment. Do not use.**

Starting a coroutine:

`go(foo(1, 2, 3));`

Yield execution to a different coroutine:

`int rc = yield();`

Create a channel:

`chan ch = chmake(item_size, buffer_size);`

Duplicate a channel handle:

`chan ch2 = chdup(ch);`

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

Mark a channel as closed for sending:

```
int val = -1;
int rc = chdone(ch, &val, sizeof(val));
```

Close a channel:

`chclose(ch);`

Multipex several channel operations:

```
int val;
struct chclause clauses[] = {
    {ch1, CHOOSE_CHRECV, &val, sizeof(val)},
    {ch2, CHOOSE_CHRECV, &val, sizeof(val)}
};
int rc = choose(clauses, 2, -1);
```

Sleep:

`int rc = msleep(now() + 1000);`

Wait for a file descriptor:

`int rc = fdwait(fd, FDW_IN | FDW_OUT, -1);`

Coroutine-local storage:

```
void *val = ...;
setcls(val);
void *val2 = cls();
```

Debugging:

```
gotrace(1);
goredump();
```
