# Threads

You can use libdill in multi-threaded programs. However, individual threads are strictly separated. You may think of each thread as a separate process.

In particular, a coroutine created in a thread will be executed in that same thread, and it will never migrate to a different one.

In a similar manner, a handle, such as a channel or a coroutine handle, created in one thread cannot be used in a different thread.
