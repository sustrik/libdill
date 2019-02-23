#!/usr/bin/env python3

import glob
from tiles import t
from schema import Schema, Optional, Or

# TODO: get rid of this
def trimrect(s):
    return ""

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
    # if true, generates function signature into header file
    Optional("signature", default=True): bool,
    # if true, generates boiles plate code for the function
    Optional("boilerplate", default=True): bool,
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
for topic, flist in topics.items():
    flist.sort(key=lambda f : f["name"])

# Enhance the data.
for fx in fxs:
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

# Generate table of contents
toc = ""
topic_order = [
    "Coroutines",
    "Deadlines",
    "Channels",
    "Handles",
    "File descriptors",
    "Bytestream sockets",
    "Message sockets",
    "IP addresses",
    "Happy Eyeballs protocol",
    "HTTP protocol",
    "IPC protocol",
    "PREFIX protocol",
    "SUFFIX protocol",
    "TCP protocol",
    "TCPMUX protocol",
    "TERM protocol",
    "TLS protocol",
    "UDP protocol",
    "WebSocket protocol"]
for topic in topics:
    if topic not in topic_order:
        raise ValueError("Topic %s missing in the topic order" % topic)
toc = t/""
for topic in topic_order:
    flist = [f["name"] for f in topics[topic]]
    flist.sort()
    items = (t/"").vjoin([t/"[@{f}(3)](@{f}.html)" for f in flist])
    toc |= t%'#### @{topic}\n' | items | t%''

with open("toc.md", 'w') as f:
    f.write(str(toc))

# Generate manpages for individual functions.
for fx in fxs:

    # SYNOPSIS section
    synopsis = t/'#include<@{fx["header"]}>'
    if fx["add_to_synopsis"]:
        synopsis |= t%'' | t/'@{fx["add_to_synopsis"]}'
    synopsis |= t%'' | t/'@{signature(fx)}'

    # DESCRIPTION section
    description = t/'@{fx["prologue"]}'
    if fx["protocol"]:
        description = t/'@{fx["protocol"]["info"]}' | t%'' | description
    if fx["experimental"] or (fx["protocol"] and fx["protocol"]["experimental"]):
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
    if fx["protocol"] or fx["topic"] == "IP addresses":
        description |= t%'' | t/'This function is not available if libdill is compiled with **--disable-sockets** option.'
    if fx["protocol"] and fx["protocol"]["topic"] == "TLS protocol":
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
    example = ""
    if fx["protocol"] and fx["protocol"]["example"]:
        example = t/'@{fx["protocol"]["example"]}'
    if fx["example"]:
        example = t/'@{fx["example"]}'
    if example:
        page |= t%'' | t/"""
            # EXAMPLE

            ```c
            @{example}
            ```
            """

    # SEE ALSO section.
    # It'll contain all funrction from the same topic plus the functions
    # added manually.
    sa = [f["name"] for f in topics[fx["topic"]] if f["name"] != fx["name"]]
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
    # Remove duplicates and list them in alphabetical order.
    sa = list(set(sa))
    sa.sort()
    #seealso = t/""
    #for f in sa:
    #    seealso += t/"**@{f}**(3) "
    seealso = (t%' ').join([t/'**@{f}**(3)' for f in sa])
    page |= t%'' | t/"""
        # SEE ALSO

        @{seealso}
        """

    with open(fx["name"] + ".md", 'w') as f:
        f.write(str(page))

# Generate header files.
hdrs = t/''
for topic in topic_order:
    signatures = t/""
    for fx in topics[topic]:
        if fx["header"] == "libdill.h" and fx["signature"]:
            signatures |= t%'' | t/'@{signature(fx, prefix="DILL_EXPORT", code=True)}'
    defines = (t/"").vjoin([t/'#define @{tp["name"]} dill_@{tp["name"]}' for tp in topics[topic]])
    signatures = t/"""
        @{signatures}

        #if !defined DILL_DISABLE_RAW_NAMES
        @{defines}
        #endif
        """
    if fx["protocol"]:
        signatures = t/"""
            #if !defined DILL_DISABLE_SOCKETS

            @{signatures}

            #endif
            """
    if topic == "TLS protocol":
        signatures = t/"""
            #if !defined DILL_DISABLE_TLS
            @{signatures}
            #endif
            """
    hdrs |= t%'' | t/'/* @{topic} */' | t%'' | t/'@{signatures}'
with open("libdill.tile.h", 'r') as f:
    c = f.read()
with open("../libdill.h", 'w') as f:
    f.write(str(t/c))

