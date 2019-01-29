fxs = [
    {
        name: "hclose",
        section: "Handles",
        info: "hard-closes a handle",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "h",
                type: "int",
                info: "The handle.",
            },
        ],
    
        has_handle_argument: true,

        prologue: `
            This function closes a handle. Once all handles pointing to the same
            underlying object have been closed, the object is deallocated
            immediately, without blocking.

            The  function  guarantees that all associated resources are
            deallocated. However, it does not guarantee that the handle's work
            will have been fully finished. E.g., outbound network data may not
            be flushed.

            In the case of network protocol sockets the entire protocol stack
            is closed, the topmost protocol as well as all the protocols
            beneath it.
        `,

        example: `
            int ch[2];
            chmake(ch);
            hclose(ch[0]);
            hclose(ch[1]);
        `
    },
    {
        name: "hown",
        section: "Handles",
        info: "transfer ownership of a handle",

        result: {
            type: "int",
            success: "new handle",
            error: "-1",
        },
        args: [
            {
                name: "h",
                type: "int",
                info: "Handle to transfer.",
            }
        ],

        allocates_handle: true,

        prologue: `
            Handles are integers. You own a handle if you know its numeric
            value. To transfer ownership this function changes the number
            associated with the object. The old number becomes invalid and
            using it will result in undefined behavior. The new number can
            be used in exactly the same way as the old one would.

            If the function fails, the old handle is closed.
        `,
        errors: ["EBADF"],

        example: `
            int h1 = tcp_connect(&addr, deadline);
            int h2 = hown(h1);
            /* h1 is invalid here */
            hclose(h2);        `
    },
    {
        name: "hmake",
        section: "Handles",
        info: "creates a handle",
        is_in_libdillimpl: true,
        add_to_synopsis: `
            struct hvfs {
                void *(*query)(struct hvfs *vfs, const void *type);
                void (*close)(int h);
                int (*done)(struct hvfs *vfs, int64_t deadline);
            };
        `,
        result: {
            type: "int",
            success: "newly created handle",
            error: "-1",
        },
        args: [
            {
                name: "hvfs",
                type: "struct hvfs*",
                info: "virtual-function table of operations associated with " +
                      "the handle",
            }
        ],

        prologue: `
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
        `,

        errors: ["ECANCELED", "EINVAL", "ENOMEM"],
    },
    {
        name: "hquery",
        section: "Handles",
        info: "gets an opaque pointer associated with a handle and a type",
        is_in_libdillimpl: true,

        result: {
            type: "void*",
            success: "opaque pointer",
            error: "NULL",
        },
        args: [
            {
                name: "h",
                type: "int",
                info: "The handle.",
            }
        ],

        prologue: `
            Returns an opaque pointer associated with the passed handle and
            type.  This function is a fundamental construct for building APIs
            on top of handles.

            The type argument is not interpreted in any way. It is used only
            as an unique ID.  A unique ID can be created, for instance, like
            this:

            \`\`\`c
            int foobar_placeholder = 0;
            const void *foobar_type = &foobar_placeholder;
            \`\`\`

            The  return value has no specified semantics. It is an opaque
            pointer.  One typical use case for it is to return a pointer to a
            table of function pointers.  These function pointers can then be
            used to access the handle's functionality (see the example).

            Pointers returned by **hquery** are meant to be cachable. In other
            words, if you call hquery on the same handle with the same type
            multiple times, the result should be the same.
        `,

        errors: ["EBADF"],

        custom_errors: {
            ENOTSUP: "The object doesn't support the supplied type.",
        },

        example: `
            struct quux_vfs {
                int (*foo)(struct quux_vfs *vfs, int a, int b);
                void (*bar)(struct quux_vfs *vfs, char *c);
            };

            int quux_foo(int h, int a, int b) {
                struct foobar_vfs *vfs = hquery(h, foobar_type);
                if(!vfs) return -1;
                return vfs->foo(vfs, a, b);
            }

            void quux_bar(int h, char *c) {
                struct foobar_vfs *vfs = hquery(h, foobar_type);
                if(!vfs) return -1;
                vfs->bar(vfs, c);
            }
        `
    },
]
