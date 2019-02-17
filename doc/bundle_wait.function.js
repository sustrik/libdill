
newFunction(
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
    }
)
