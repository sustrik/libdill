# Threads

You can use libdill in multi-threaded programs. However, there is strict separation between individual threads. You can think of each thread as of separate process.

In particular, a coroutine created in a thread will be executed in that thread. It will never migrate to a different thread.

In a similar manner, a handle, such as channel or coroutine handle, created in one thread cannot be used in a different thread.
