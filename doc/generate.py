#!/usr/bin/env python3

import glob
from tiles import t
from schema import Schema, Optional, Or

# Use dill_ prefix depending on whether the result goes into docs or code.
def dillify(t, dill):
    if not dill:
        return t
    parts = t.split()
    if len(parts) and parts[0] == "struct":
        return "struct dill_" + " ".join(parts[1:])
    elif len(parts) >= 2 and parts[0] == "const" and parts[1] == "struct":
        return "const struct dill_" + " ".join(parts[2:])
    else:
        return "dill_" + t

def signature(fx, prefix="", code=False):
    args = "void);"
    if len(fx["args"]) > 0:
        if code:
            fmt = '@{dillify(arg["type"], arg["dill"])} @{arg["name"]}@{arg["suffix"]}'
        else:
            fmt = '@{arg["type"]} @{arg["name"]}@{arg["suffix"]}'
        args = (t/",").vjoin([t/fmt for arg in fx["args"]], last=t/");")
    return t/"""
             @{prefix} @{fx["result"]["type"] if fx["result"] else "void"} @{"dill_" if code else ""}@{fx["name"]}(
                 @{args}
             """

print("Loading")

topics = {}

def new_topic(topic):
    if topic["name"] in topics:
        raise Exception("Duplicate topic name: " + topic.name)
    topics[topic["name"]] = topic

files = glob.glob("*.topic.py")
for file in files:
    print("  " + file)
    with open(file, 'r') as f:
        c = f.read()
        exec(c)

for _, topic in topics.items():
    topic["functions"] = {}

def new_function(fx):
    if fx["topic"] not in topics:
        raise Exception("Unknown topic: " + fx["topic"])
    fxs = topics[fx["topic"]]["functions"]
    if fx["name"] in fxs:
        raise Exception("Duplicate function name: " + fx["name"])
    fxs[fx["name"]] = fx

files = glob.glob("*.function.py")
for file in files:
    print("  " + file)
    with open(file, 'r') as f:
        c = f.read()
        exec(c)

print("Validating")

fschema = {
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
        # set to true for dill-specific types
        Optional("dill", default=False): bool,
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
    # if true, generates function signature into header file
    Optional("signature", default=True): bool,
    # if true, generates boiles plate code for the function
    Optional("boilerplate", default=True): bool,
}

tschema = {
    # short name of the protocol; typically the function prefix
    "name": str,
    # title of the topic as it appears in the ToC
    "title": str,
    # topic with smaller order numbers will appear first in the ToC
    Optional("order", default=None): int,
    # if the topic is about a protocol, the type of the protocol
    Optional("protocol", default=None): Or("bytestream", "message", "application"),
    # this string will be added to the man page of each function in the topic
    Optional("info", default=None): str,
    # this example will be used for the functions that don't have example of their own
    Optional("example", default=None): str,
    # all functions in the topic
    "functions": {str: fschema},
    # option types associated with this topic
    Optional("opts", default=[]): [str],
    # storage types associated with this topic (value is the size of the structure in bytes)
    Optional("storage", default={}): {str: int},
    # true if the functionality is experimental
    Optional("experimental", default=False): bool,
}

topics = Schema({str: tschema}).validate(topics)

print("Enriching data")

for _, topic in topics.items():
    for _, fx in topic["functions"].items():
        if fx["has_iol"]:
            fx["args"].append({
                "name": "first",
                "type": "struct iolist*",
                "dill": True,
                "info": "Pointer to the first item of a linked list of I/O buffers.",
                "suffix": "",
            })
            fx["args"].append({
                "name": "last",
                "type": "struct iolist*",
                "dill": True,
                "info": "Pointer to the last item of a linked list of I/O buffers.",
                "suffix": "",
            })

        if fx["has_deadline"]:
            fx["args"].append({
                "name": "deadline",
                "type": "int64_t",
                "dill": False,
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

print("Generating table of contents")

# Order the topics. Topic with "order" field go first.
# Other topics go afterwards, in alphabetical order.
ordered = [(name, topic["order"]) for name, topic in topics.items() if topic["order"] != None]
ordered = sorted(ordered, key=lambda x: x[1])
ordered = [x[0] for x in ordered]
unordered = [(name, topic["title"]) for name, topic in topics.items() if topic["order"] == None]
unordered = sorted(unordered, key=lambda x: x[1])
unordered = [x[0] for x in unordered]
order = ordered + unordered

toc = t/""
for topic in order:
    toc |= t%'#### @{topics[topic]["title"]}' | t%''
    for name, _ in sorted(topics[topic]["functions"].items()):
        toc |= t/"[@{name}(3)](@{name}.html)"
    toc |= t%''

with open("toc.md", 'w') as f:
    f.write(str(toc))

print("Generating man pages")

for _, topic in topics.items():
    for _, fx in topic["functions"].items():

        # SYNOPSIS section
        synopsis = t/'#include<@{fx["header"]}>'
        if fx["add_to_synopsis"]:
            synopsis |= t%'' | t/'@{fx["add_to_synopsis"]}'
        synopsis |= t%'' | t/'@{signature(fx)}'

        # DESCRIPTION section
        description = t/'@{fx["prologue"]}'
        if topic["info"]:
            description = t/'@{topic["info"]}' | t%'' | description
        if fx["experimental"] or topic["experimental"]:
            description = t/'**WARNING: This is experimental functionality and the API may change in the future.**' | t%'' | description
        if fx["has_iol"]:
            description |= t%'' | t/"""
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
                """
        if fx["args"]:
            description |= t%'' | (t/'').vjoin([t/'**@{arg["name"]}**: @{arg["info"]}' for arg in fx["args"]])
        if fx["epilogue"]:
            description |= t%'' | t/'@{fx["epilogue"]}'
        if topic["protocol"] or topic["name"] == "ipaddr":
            description |= t%'' | t/'This function is not available if libdill is compiled with **--disable-sockets** option.'
        if topic["name"] == "tls":
            description |= t%'' | t/'This function is not available if libdill is compiled without **--enable-tls** option.'

        # RETURN VALUE section
        if fx["result"]:
            if fx["result"]["success"] and fx["result"]["error"]:
                retval = t/"""
                    In case of success the function returns @{fx["result"]["success"]}.
                    'In case of error it returns @{fx["result"]["error"]} and sets **errno** to one of the values below.
                    """
            if fx["result"]["info"]:
                retval = t/'@{fx["result"]["info"]}'
        else:
            retval = t/"None."

        # ERRORS section
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
        errors = t/"None."
        if len(errs) > 0:
            errors = (t/'').vjoin([t/'* **@{e}**: @{desc}' for e, desc in sorted(errs.items())])
        if fx["add_to_errors"]:
            errors |= t%'' | t/'@{fx["add_to_errors"]}'

        # Generate the manpage.
        page = t/"""
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
                 """

        # Add EXAMPLE section, if available.
        example = topic["example"]
        if fx["example"]:
            example = fx["example"]
        if example:
            page |= t%'' | t/"""
                # EXAMPLE

                ```c
                @{example}
                ```
                """

        # SEE ALSO section.
        # It'll contain all functions from the same topic plus the functions
        # added manually.
        sa = [f for f in topic["functions"] if f != fx["name"]]
        if fx["has_deadline"]:
            sa.append("now")
        if fx["allocates_handle"]:
            sa.append("hclose")
        if topic["protocol"] == "bytestream":
            sa.append("brecv")
            sa.append("brecvl")
            sa.append("bsend")
            sa.append("bsendl")
        if topic["protocol"] == "message":
            sa.append("mrecv")
            sa.append("mrecvl")
            sa.append("msend")
            sa.append("msendl")
        # Remove duplicates and list them in alphabetical order.
        sa = list(set(sa))
        sa.sort()
        seealso = (t%' ').join([t/'**@{f}**(3)' for f in sa])
        page |= t%'' | t/"""
            # SEE ALSO

            @{seealso}
            """

        with open(fx["name"] + ".md", 'w') as f:
            f.write(str(page))

print("Generating header files")

hdrs = t/''
for topic in order:
    fxs = topics[topic]["functions"]
    signatures = t/""
    for _, fx in fxs.items():
        if fx["header"] == "libdill.h" and fx["signature"]:
            signatures |= t%'' | t/'@{signature(fx, prefix="DILL_EXPORT", code=True)}'
    defines = (t/"").vjoin([t/'#define @{fx["name"]} dill_@{fx["name"]}' for _, fx in fxs.items()])
    signatures = t/"""
        @{signatures}

        #if !defined DILL_DISABLE_RAW_NAMES
        @{defines}
        #endif
        """
    if topics[topic]["protocol"]:
        signatures = t/"""
            #if !defined DILL_DISABLE_SOCKETS

            @{signatures}

            #endif
            """
    if topics[topic]["name"] == "tls":
        signatures = t/"""
            #if !defined DILL_DISABLE_TLS
            @{signatures}
            #endif
            """
    hdrs |= t%'' | t/'/* @{topics[topic]["title"]} */' | t%'' | t/'@{signatures}'
with open("libdill.tile.h", 'r') as f:
    c = f.read()
with open("../libdill.h", 'w') as f:
    f.write(str(t/c))

