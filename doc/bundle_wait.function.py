
bundle_wait_function = {
    "name": "bundle_wait",
    "topic": "cr",
    "info": "wait while coroutines in the bundle finish",
    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },
    "args": [
        {
            "name": "bndl",
            "type": "int",
            "info": "Handle of a coroutine bundle.",
        },
    ],
    "has_deadline": True,
    "has_handle_argument": True,

    "prologue": """
        If there are no coroutines in the bundle the function will succeed
        immediately. Otherwise, it will wait until all the coroutines in
        the bundle finish or until deadline is reached.
    """,

    "example": bundle_example,
}


new_function(bundle_wait_function)
