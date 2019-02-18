#!/usr/bin/env python3

import glob
import tiles
from schema import Schema, Optional, Or

def trimrect(s):
    return ""

# Read all topic files.
files = glob.glob("*.topic.py")
for file in files:
    with open(file, 'r') as f:
        c = f.read()
        exec(c)

# Read all topic files.
fxs = []
files = glob.glob("*.function.py")
for file in files:
    with open(file, 'r') as f:
        c = f.read()
        exec(c)

# Check for duplicate functions.
names = {}
for fx in fxs:
    if fx["name"] in names:
        raise ValueError("Duplicate function name %s" % fx["name"])

schema = Schema([{
    # name of the function
    "name": str,
    # the name of the section in the docs the function should appear in
    # if omitted, it will be retrieved from the protocol
    Optional("topic", default=None): str,
    # short description of the function
    "info": str,
    # which headerfile is the funcion declared in
    Optional("header", default='libdill.h'): str,
    # arguments of the function
    "args": [{
        # name of the argument
        "name": str,
        # type of the argument (omit for expressions)
        Optional("type", default=""): str,
        # part of the type to go after the name (e.g. "[]")
        Optional("suffix", default=""): str,
        # description of the argument
        "info": str,
    }],
    # omit this field for void functions
    Optional("result", default=None): {
        # return type of the function
        "type": str,
        Optional("success", default=None): str,
        Optional("error", default=None): str,
        Optional("info", default=None): str,
    },
    # the part of the description of the function to go before the list
    # of arguments
    "prologue": str,
    # the part of the description of the function to go after the list
    # of arguments
    Optional("epilogue", default=None): str,
    # should be present only if the function is related to a network protocol
    Optional("protocol", default=None): {
        # the section in the docs the protocol belongs to
        "topic": str,
        # type of the protocol
        "type": Or("bytestream", "message", "application"),
        # description of the protocol
        "info": str,
        # example of usage of the protocol, a piece of C code
        "example": str,
        # if true, the docs will contain a warning about using this protocol
        Optional("experimental", default=False): bool,
    },
    # a piece of code to be added to synopsis, between the include
    # and the function declaration
    Optional("add_to_synopsis", default=None): str,
    Optional("add_to_errors", default=None): str,
    # set to true is the function allocates at least one handle
    Optional("allocates_handle", default=False): bool,
    # set to true if at least one of the arguments is a handle
    Optional("has_handle_argument", default=False): bool,
    # if true, adds deadline to the list of arguments
    Optional("has_deadline", default=False): bool,
    Optional("uses_connection", default=False): bool,
    # if true, adds I/O list to the list of arguments
    Optional("has_iol", default=False): bool,
    # if set, _mem variant of the function will be generated;
    # value if the name of the storage struct, e.g. "tcp_storage".
    Optional("mem"): str,
    # a list of possible errors, the descriptions will be pulled from
    # standard_errors object
    Optional("errors", default=[]): [str],
    # custom errors take precedence over standard errors
    # value maps error names to error descriptions
    Optional("custom_errors", default={}): {str: str},
    # example of the usage of the function, a piece of C code;
    # if present, it overrides the protocol example
    Optional("example", default=None): str,
    # if true, the docs will contain a warning about using this function
    Optional("experimental", default=False): bool,
}])

# Check whether the data comply to the schema. Also fills in defaults.
fxs = schema.validate(fxs)

# Collect the topics.
topics = {}
for fx in fxs:
    if fx["topic"]:
        topic = fx["topic"]
    elif fx["protocol"]:
        topic = fx["protocol"]["topic"]
        fx["topic"] = topic
    else:
        raise ValueError("Missing topic in %s" % fx["name"])
    if topic not in topics:
        topics[topic] = []
    topics[topic].append(fx)

# Enhance the data.
for fx in fxs:
    if fx["has_iol"]:
        fx["args"].append({
            "name": "first",
            "type": "struct iolist*",
            "info": "Pointer to the first item of a linked list of I/O buffers.",
            "suffix": "",
        })
        fx["args"].append({
            "name": "last",
            "type": "struct iolist*",
            "info": "Pointer to the last item of a linked list of I/O buffers.",
            "suffix": "",
        })

    if fx["has_deadline"]:
        fx["args"].append({
            "name": "deadline",
            "type": "int64_t",
            "info": "A point in time when the operation should time out, in " +
                    "milliseconds. Use the **now** function to get your current" +
                    " point in time. 0 means immediate timeout, i.e., perform " +
                    "the operation if possible or return without blocking if " +
                    "not. -1 means no deadline, i.e., the call will block " +
                    "forever if the operation cannot be performed.",
            "suffix": "",
        })

    if fx["has_handle_argument"]:
        fx["errors"].append("EBADF")
        fx["errors"].append("ENOTSUP")
    if fx["has_deadline"]:
        fx["errors"].append("ECANCELED")
        if fx["name"] != "msleep":
            fx["errors"].append("ETIMEDOUT")
    if fx["allocates_handle"]:
        fx["errors"].append("EMFILE")
        fx["errors"].append("ENFILE")
        fx["errors"].append("ENOMEM")
    if fx["uses_connection"]:
        fx["errors"].append("ECONNRESET")
        fx["errors"].append("ECANCELED")
    # TODO: if(fx.mem && !mem) errs.push("ENOMEM")

# Generate manpages for individual functions.
for fx in fxs:

    args = "void"
    if len(fx["args"]) > 0:
        args = tiles.joinv(fx["args"], "@{type} @{name}@{suffix}", sep=", ", last=");")

    signature = tiles.tile(
        """
        @{fx["result"]["type"] if fx["result"] else "void"} @{fx["name"]}(
            @{args}
        """)
    
    synopsis = tiles.tile("#include<@{fx['header']}>")
    if fx["add_to_synopsis"]:
        synopsis = tiles.tile('@{synopsis}\n\n@{fx["add_to_synopsis"]}');
    synopsis = tiles.tile('@{synopsis}\n\n@{signature}')

    description = ""
    if fx["experimental"] or (fx["protocol"] and fx["protocol"]["experimental"]):
        description = "**WARNING: This is experimental functionality and the API may change in the future.**"
    if fx["protocol"]:
        description = tiles.tile('@{description}\n\n@{fx["protocol"]["info"]}')
    if fx["prologue"]:
        description = tiles.tile('@{description}\n\n@{fx["prologue"]}')
    if fx["has_iol"]:
        description = tiles.tile(
            """
            @{description}

            This function accepts a linked list of I/O buffers instead of a
            single buffer. Argument **first** points to the first item in the
            list, **last** points to the last buffer in the list. The list
            represents a single, fragmented message, not a list of multiple
            messages. Structure **iolist** has the following members:

            ```c
            void *iol_base;          /* Pointer to the buffer. */
            size_t iol_len;          /* Size of the buffer. */
            struct iolist *iol_next; /* Next buffer in the list. */
            int iol_rsvd;            /* Reserved. Must be set to zero. */
            ```

            When receiving, **iol_base** equal to NULL means that **iol_len**
            bytes should be skipped.

            The function returns **EINVAL** error in the case the list is
            malformed:

            * If **last->iol_next** is not **NULL**.
            * If **first** and **last** don't belong to the same list.
            * If there's a loop in the list.
            * If **iol_rsvd** of any item is non-zero.

            The list (but not the buffers themselves) can be temporarily
            modified while the function is in progress. However, once the
            function returns the list is guaranteed to be the same as before
            the call.
            """)
    arginfo = tiles.joinv(fx["args"], "**@{name}**: @{info}", sep="\n")
    description = tiles.tile('@{description}\n\n@{arginfo}')
    if fx["epilogue"]:
        description = tiles.tile('@{description}\n\n@{fx["epilogue"]}')
    if fx["protocol"] or fx["topic"] == "IP addresses":
        description = tiles.tile('@{description}\n\nThis function is not available if libdill is compiled with **--disable-sockets** option.')
    if fx["protocol"] and fx["protocol"]["topic"] == "TLS protocol":
        description = tiles.tile('@{description}\n\nThis function is not available if libdill is compiled without **--enable-tls** option.')

    if fx["result"]:
        retval = ""
        if fx["result"]["success"] and fx["result"]["error"]:
            retval = tiles.tile('@{retval}\n\nIn case of success the function returns @{fx["result"]["success"]}. ' +
                'In case of error it returns @{fx["result"]["error"]} and sets **errno** to one of the values below.}')
        if fx["result"]["info"]:
            retval = tiles.tile('@{retval}\n\n@{fx["result"]["info"]}')
    else:
        retval = "None."

    standard_errors = {
        "EBADF": "Invalid handle.",
        "EBUSY": "The handle is currently being used by a different coroutine.",
        "ETIMEDOUT": "Deadline was reached.",
        "ENOMEM": "Not enough memory.",
        "EMFILE": "The maximum number of file descriptors in the process are already open.",
        "ENFILE": "The maximum number of file descriptors in the system are already open.",
        "EINVAL": "Invalid argument.",
        "EMSGSIZE": "The data won't fit into the supplied buffer.",
        "ECONNRESET": "Broken connection.",
        "ECANCELED": "Current coroutine was canceled.",
        "ENOTSUP": "The handle does not support this operation.",
    }
    errs = {}
    for e in fx["errors"]:
        errs[e] = standard_errors[e]
    errs.update(fx["custom_errors"])
    errors = "None."
    if len(errs) > 0:
        errors = ""
        for e, desc in sorted(errs.items()):
            errors += tiles.tile("* **@{e}**: @{desc}\n")
    if fx["add_to_errors"]:
        errors = tiles.tile('@{errors}\n\n@{fx["add_to_errors"]}')

    page = tiles.tile(
        """
        # NAME

        @{fx["name"]} - @{fx["info"]}

        # SYNOPSIS

        ```c
        @{synopsis}
        ```

        # DESCRIPTION

        @{description}

        # RETURN VALUE

        @{retval}

        # ERRORS

        @{errors}
        """);

    example = ""
    if fx["protocol"] and fx["protocol"]["example"]:
        example = fx["protocol"]["example"]
    if fx["example"]:
        example = fx["example"]
    if example:
        page = tiles.tile(
            """
            @{page}

            # EXAMPLE

            ```c
            @{example}
            ```
            """)

    # put all functions from the same topc into "see also" section
    sa = [f["name"] for f in topics[fx["topic"]] if f["name"] != fx["name"]]
    # add special items
    if fx["has_deadline"]:
        sa.append("now")
    if fx["allocates_handle"]:
        sa.append("hclose")
    if fx["protocol"] and fx["protocol"]["type"] == "bytestream":
        sa.append("brecv")
        sa.append("brecvl")
        sa.append("bsend")
        sa.append("bsendl")
    if fx["protocol"] and fx["protocol"]["type"] == "message":
        sa.append("mrecv")
        sa.append("mrecvl")
        sa.append("msend")
        sa.append("msendl")
    # remove duplicates, list in alphabetical order
    sa = list(set(sa))
    sa.sort()
    seealso = ""
    for f in sa:
        seealso += tiles.tile("**@{f}**(3) ") 

    page = tiles.tile(
        """
        @{page}

        # SEE ALSO

        @{seealso}
        """)        

    with open(fx["name"] + ".md", 'w') as f:
        f.write(tiles.tile("@{page}"))

