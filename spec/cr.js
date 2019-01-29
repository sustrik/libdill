fxs = [
    {
        name: "bundle",
        section: "Coroutines",
        info: "create an empty coroutine bundle",
        result: {
            type: "int",
            success: "handle of the newly create coroutine bundle",
            error: "-1",
        },

        prologue: `
            Coroutines are always running in bundles. Even a single coroutine
            created by **go** gets its own bundle. A bundle is a lifetime
            control mechanism. When it is closed, all coroutines within the
            bundle are canceled.

            This function creates an empty bundle. Coroutines can be added to
            the bundle using the **bundle_go** and **bundle_go_mem** functions.

            To wait until coroutines in the bundle finish use **bundle_wait**
            function.

            When **hclose()** is called on the bundle, all the coroutines
            contained in the bundle will be canceled. In other words, all the
            blocking functions within the coroutine will start failing with an
            **ECANCELED** error. The **hclose()** function itself won't exit
            until all the coroutines in the bundle exit.
        `,

        allocates_handle: true,

        example: bundle_example,

        mem: "bundle_storage",
    },
    {
        name: "bundle_go",
        section: "Coroutines",
        info: "launches a coroutine within a bundle",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "bndl",
                type: "int",
                info: "Bundle to launch the coroutine in.",
            },
            {
                name: "expression",
                info: "Expression to evaluate as a coroutine.",
            },
        ],

        prologue: `
            This construct launches a coroutine within the specified bundle.
            For more information about bundles see **bundle**.

            The coroutine gets a 1MB stack.
            The stack is guarded by a non-writeable memory page. Therefore,
            stack overflow will result in a **SEGFAULT** rather than overwriting
            memory that doesn't belong to it.
        `,
        epilogue: go_info,

        has_handle_argument: true,

        errors: ["ECANCELED", "ENOMEM"],

        example: bundle_example,
    },
    {
        name: "bundle_wait",
        section: "Coroutines",
        info: "wait while coroutines in the bundle finish",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "bndl",
                type: "int",
                info: "Handle of a coroutine bundle.",
            },
        ],
        has_deadline: true,
        has_handle_argument: true,

        prologue: `
            If there are no coroutines in the bundle the function will succeed
            immediately. Otherwise, it will wait until all the coroutines in
            the bundle finish or until deadline is reached.
        `,

        example: bundle_example,
    },
    {
        name: "bundle_go_mem",
        section: "Coroutines",
        info: "launches a coroutine within a bundle",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "bndl",
                type: "int",
                info: "Bundle to launch the coroutine in.",
            },
            {
                name: "expression",
                info: "Expression to evaluate as a coroutine.",
            },
            {
                name: "mem",
                type: "void*",
                info: "Buffer to store coroutine's stack.",
            },
            {
                name: "memlen",
                type: "size_t",
                info: "Size of th buffer, in bytes.",
            },
        ],

        prologue: `
            This construct launches a coroutine within the specified bundle.
            For more information about bundles see **bundle**.

            The stack for the coroutine is supplied by the user.
            The stack has no guard page and stack overflow will result in
            overwriting memory.
        `,
        epilogue: go_info,

        has_handle_argument: true,

        errors: ["ECANCELED"],

        example: bundle_example,
    },
    {
        name: "go",
        section: "Coroutines",
        info: "launches a coroutine",

        result: {
            type: "int",
            success: "handle of a bundle containing the new coroutine",
            error: "-1",
            info: "For details on coroutine bundles see **bundle** function.",
        },

        args: [
            {
                name: "expression",
                info: "Expression to evaluate as a coroutine.",
            }
        ],

        allocates_handle: true,

        prologue: `
            This construct launches a coroutine. The coroutine gets a 1MB stack.
            The stack is guarded by a non-writeable memory page. Therefore,
            stack overflow will result in a **SEGFAULT** rather than overwriting
            memory that doesn't belong to it.
        `,
        epilogue: go_info,

        errors: ["ECANCELED", "ENOMEM"],

        example: `
            coroutine void add(int a, int b) {
                printf("%d+%d=%d\\n", a, b, a + b);
            }

            ...

            int h = go(add(1, 2));
        `,
    },
    {
        name: "go_mem",
        section: "Coroutines",
        info: "launches a coroutine",

        result: {
            type: "int",
            success: "handle of a bundle containing the new coroutine",
            error: "-1",
            info: "For details on coroutine bundles see **bundle** function.",
        },

        args: [
            {
                name: "expression",
                info: "Expression to evaluate as a coroutine.",
            },
            {
                name: "mem",
                type: "void*",
                info: "Buffer to hold the coroutine's stack. The buffer " +
                      "doesn't have to be aligned. The function will take " +
                      "care of properly aligning the stack itself.",
            },
            {
                name: "memlen",
                type: "size_t",
                info: "Size of the **mem** buffer.",
            },
        ],

        allocates_handle: true,

        prologue: `
            This construct launches a coroutine on a user-supplied stack.
            The stack has no guard page and stack overflow will result in
            overwriting memory.
        `,
        epilogue: go_info,

        errors: ["ECANCELED"],

        example: `
              coroutine void add(int a, int b) {
                  printf("%d+%d=%d\\n", a, b, a + b);
              }

              ...
              char mem[16384];
              int h = go_mem(add(1, 2), mem, sizeof(mem));
        `,
    },
    {
        name: "happyeyeballs_connect",
        section: "Happy Eyeballs protocol",
        info: "creates a TCP connection using Happy Eyeballs algorithm",

        result: {
            type: "int",
            success: "newly created TCP connection",
            error: "-1",
        },
        args: [
            {
                name: "name",
                type: "const char*",
                info: "Name of the host to connect to.",
            },
            {
                name: "port",
                type: "int",
                info: "Port to connect to.",
            }
        ],
        has_deadline: true,

        prologue: `
            Happy Eyeballs protocol (RFC 8305) is a way to create a TCP
            connection to a remote endpoint even if some of the IP addresses
            associated with the endpoint are not accessible.

            First, DNS queries to IPv4 and IPv6 addresses are launched in
            parallel. Then a connection is attempted to the first address, with
            IPv6 address being preferred. If the connection attempt doesn't
            succeed in 300ms connection to next address is attempted,
            alternating between IPv4 and IPv6 addresses. However, the first
            connection attempt isn't caneled. If, in the next 300ms, none of the
            previous attempts succeeds connection to the next address is
            attempted, and so on. First successful connection attempt will
            cancel all other connection attemps.

            This function executes the above protocol and returns the newly
            created TCP connection.
        `,

        errors: ["EINVAL", "EMFILE", "ENFILE", "ENOMEM", "ECANCELED"],

        example: `
            int s = happyeyeballs_connect("www.example.org", 80, -1);
            int rc = bsend(s, "GET / HTTP/1.1", 14, -1);
        `
    },
    {
        name: "yield",
        section: "Coroutines",
        info: "yields CPU to other coroutines",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
        ],

        prologue: `
            By calling this function, you give other coroutines a chance to run.

            You should consider using **yield** when doing lengthy computations
            which don't have natural coroutine switching points such as socket
            or channel operations or msleep.
        `,

        errors: ["ECANCELED"],

        example: `
            for(i = 0; i != 1000000; ++i) {
                expensive_computation();
                yield(); /* Give other coroutines a chance to run. */
            }
        `,
    },
]
