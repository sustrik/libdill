
fxs.append(
    {
        "name": "hmake",
        "section": "Handles",
        "info": "creates a handle",
        "header": "libdillimpl.h",
        "add_to_synopsis": """
            struct hvfs {
                void *(*query)(struct hvfs *vfs, const void *type);
                void (*close)(int h);
                int (*done)(struct hvfs *vfs, int64_t deadline);
            };
        """,
        "result": {
            "type": "int",
            "success": "newly created handle",
            "error": "-1",
        },
        "args": [
            {
                "name": "hvfs",
                "type": "struct hvfs*",
                "info": "virtual-function table of operations associated with " +
                      "the handle",
            }
        ],

        "prologue": """
            A handle is the user-space equivalent of a file descriptor.
            Coroutines and channels are represented by handles.

            Unlike with file descriptors, however, you can use the **hmake**
            function to create your own type of handle.

            The argument of the function is a virtual-function table of
            operations associated with the handle.

            When implementing the **close** operation, keep in mind that
            invoking blocking operations is not allowed, as blocking operations
            invoked within the context of a **close** operation will fail with
            an **ECANCELED** error.

            To close a handle, use the **hclose** function.
        """,

        "errors": ["ECANCELED", "EINVAL", "ENOMEM"],
    }
)
