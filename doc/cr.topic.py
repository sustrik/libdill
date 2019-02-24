
bundle_example = """
    int b = bundle();
    bundle_go(b, worker());
    bundle_go(b, worker());
    bundle_go(b, worker());
    /* Give wrokers 1 second to finish. */
    bundle_wait(b, now() + 1000);
    /* Cancel any remaining workers. */
    hclose(b);
"""

go_info = """
    The coroutine is executed concurrently, and its lifetime may exceed the
    lifetime of the caller coroutine. The return value of the coroutine, if any,
    is discarded and cannot be retrieved by the caller.

    Any function to be invoked as a coroutine must be declared with the
    **coroutine** specifier.

    Use **hclose** to cancel the coroutine. When the coroutine is canceled
    all the blocking calls within the coroutine will start failing with
    **ECANCELED** error.

    "_WARNING_": Coroutines will most likely work even without the coroutine
    specifier. However, they may fail in random non-deterministic ways,
    depending on the code in question and the particular combination of compiler
    and optimization level. Additionally, arguments to a coroutine must not be
    function calls. If they are, the program may fail non-deterministically.
    If you need to pass a result of a computation to a coroutine, do the
    computation first, and then pass the result as an argument.  Instead "of":

    ```c
    go(bar(foo(a)));
    ```

    Do "this":

    ```c
    int a = foo();
    go(bar(a));
    ```
"""

cr_topic = {
    "name": "cr",
    "title": "Coroutines",
    "order": 10,
}

new_topic(cr_topic)

