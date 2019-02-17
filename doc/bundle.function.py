
fxs.append(
    {
        "name": "bundle",
        "section": "Coroutines",
        "info": "create an empty coroutine bundle",
        "args": [],
        "result": {
            "type": "int",
            "success": "handle of the newly create coroutine bundle",
            "error": "-1",
        },

        "prologue": """
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
        """,

        "allocates_handle": True,

        "example": bundle_example,

        "mem": "bundle_storage",
    }
)
