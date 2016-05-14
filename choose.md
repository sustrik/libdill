# choose(3) manual page

## NAME

choose() - perform one of multiple channel operations

## SYNOPSIS

```
#include <libdill.h>

#define CHSEND 1
#define CHRECV 2

struct chclause {
    int op;
    int ch;
    void *val;
    size_t len;
};

int choose(struct chclause *clauses, int nclauses,int64_t deadline);
```

## DESCRIPTION

## RETURN VALUE

## ERRORS

## EXAMPLE

## SEE ALSO

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

