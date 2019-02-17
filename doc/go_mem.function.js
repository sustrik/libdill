
newFunction(
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
    }
)
