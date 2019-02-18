
fxs.append(
    {
        "name": "go",
        "topic": "Coroutines",
        "info": "launches a coroutine",

        "result": {
            "type": "int",
            "success": "handle of a bundle containing the new coroutine",
            "error": "-1",
            "info": "For details on coroutine bundles see **bundle** function.",
        },

        "args": [
            {
                "name": "expression",
                "info": "Expression to evaluate as a coroutine.",
            }
        ],

        "allocates_handle": True,

        "prologue": """
            This construct launches a coroutine. The coroutine gets a 1MB stack.
            The stack is guarded by a non-writeable memory page. Therefore,
            stack overflow will result in a **SEGFAULT** rather than overwriting
            memory that doesn't belong to it.
        """,
        "epilogue": go_info,

        "errors": ["ECANCELED", "ENOMEM"],

        "example": """
            coroutine void add(int a, int b) {
                printf("%d+%d=%d\\n", a, b, a + b);
            }

            ...

            int h = go(add(1, 2));
        """,
    }
)
