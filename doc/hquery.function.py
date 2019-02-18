
fxs.append(
    {
        "name": "hquery",
        "topic": "Handles",
        "info": "gets an opaque pointer associated with a handle and a type",
        "header": "libdillimpl.h",

        "result": {
            "type": "void*",
            "success": "opaque pointer",
            "error": "NULL",
        },
        "args": [
            {
                "name": "h",
                "type": "int",
                "info": "The handle.",
            },
            {
                "name": "type",
                "type": "const void*",
                "info": "Opaque ID of the handle type.",
            },
        ],

        "prologue": """
            Returns an opaque pointer associated with the passed handle and
            type.  This function is a fundamental construct for building APIs
            on top of handles.

            The type argument is not interpreted in any way. It is used only
            as an unique ID.  A unique ID can be created, for instance, like
            "this":

            ```c
            int foobar_placeholder = 0;
            const void *foobar_type = &foobar_placeholder;
            ```

            The  return value has no specified semantics. It is an opaque
            pointer.  One typical use case for it is to return a pointer to a
            table of function pointers.  These function pointers can then be
            used to access the handle's functionality (see the example).

            Pointers returned by **hquery** are meant to be cachable. In other
            words, if you call hquery on the same handle with the same type
            multiple times, the result should be the same.
        """,

        "errors": ["EBADF"],

        "custom_errors": {
            "ENOTSUP": "The object doesn't support the supplied type.",
        },

        "example": """
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
        """
    }
)
