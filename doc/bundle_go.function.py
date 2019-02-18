
fxs.append(
    {
        "name": "bundle_go",
        "section": "Coroutines",
        "info": "launches a coroutine within a bundle",

        "result": {
            "type": "int",
            "success": "0",
            "error": "-1",
        },

        "args": [
            {
                "name": "bndl",
                "type": "int",
                "info": "Bundle to launch the coroutine in.",
            },
            {
                "name": "expression",
                "info": "Expression to evaluate as a coroutine.",
            },
        ],

        "prologue": """
            This construct launches a coroutine within the specified bundle.
            For more information about bundles see **bundle**.

            The coroutine gets a 1MB stack.
            The stack is guarded by a non-writeable memory page. Therefore,
            stack overflow will result in a **SEGFAULT** rather than overwriting
            memory that doesn't belong to it.
        """,
        "epilogue": go_info,

        "has_handle_argument": True,

        "errors": ["ECANCELED", "ENOMEM"],

        "example": bundle_example,
    }
)
