
Libdill is a C library that makes writing [structured concurrent programs](structured-concurrency.html) easy.

The following example launches two concurrent worker functions that print "Hello!" or "World!", respectively, in random intervals. Program runs for five seconds, then it shuts down.

```c
#include <libdill.h>
#include <stdio.h>
#include <stdlib.h>

coroutine int worker(const char *text) {
    while(1) {
        printf("%s\n", text);
        msleep(now() + random() % 500);
    }
    return 0;
}

int main() {
    go(worker("Hello!"));
    go(worker("World!"));
    msleep(now() + 5000);
    return 0;
}
```

Code using libdill is compiled in standard C way. The only additional requirement is to link it with libdill library:

```
$ cc -ldill -o hello hello.c
```

Libdill is licensed under MIT/X11 license.

