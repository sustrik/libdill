#!/usr/bin/env python3

import glob
from schema import Schema, And, Use, Optional

def trimrect(s):
    return ""

# Read all topic files.
fxs = []
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

# Validate the data.
schema = Schema([{
    # name of the function
    "name": str,
    # the name of the section the function belongs to
    # if omitted, the section is retrieved from the protocol
    Optional("section"): str,
    # short description of the function
    "info": str,
    # if true, the function is in libdillimpl.h
    Optional("is_in_libdillimpl"): bool,
    # arguments of the function
    "args": [{
        # name of the argument
        "name": str,
        # type of the argument (for expressions use "")
        "type": str,
        # part of the type to go after the name (e.g. "[]")
        Optional("suffix"): str,
        # description of the argument
        "info": str,
    }],
    # omit this field for void functions
    Optional("result"): {
        # return type of the function
        "type": str,
        Optional("success"): str,
        Optional("error"): str,
        Optional("info"): str,
    },
    # the part of the description of the function to go before the list
    # of arguments
    "prologue": str,
    # the part of the description of the function to go after the list
    # of arguments
    Optional("epilogue"): str,
    # should be present only if the function is related to a network protocol
    Optional("protocol"): {
        # the section that describes the protocol
        "section": str,
        # type of the protocol (bytestream or message)
        "type": str,
        # description of the protocol
        "info": str,
        # example of usage of the protocol, a piece of C code
        "example": str,
        # if true, the docs will contain a warning about using this protocol
        Optional("experimental"): bool,
    },
    # a piece of code to be added to synopsis, between the include
    # and the function declaration
    Optional("add_to_synopsis"): str,
    Optional("add_to_errors"): str,
    Optional("has_handle_argument"): bool,
    # set to true is the function allocates at least one handle
    Optional("allocates_handle"): bool,
    # set to true if at least one of the arguments is a handle
    Optional("has_handle_argument"): bool,
    # if true, adds deadline to the list of arguments
    Optional("has_deadline"): bool,
    Optional("uses_connection"): bool,
    # if true, adds I/O list to the list of arguments
    Optional("has_iol"): bool,
    # if set, _mem variant of the function will be generated;
    # value if the name of the storage struct, e.g. "tcp_storage".
    Optional("mem"): str,
    # a list of possible errors, the descriptions will be pulled from
    # standard_errors object
    Optional("errors"): [str],
    # custom errors take precedence over standard errors
    # value maps error names to error descriptions
    Optional("custom_errors"): {str: str},
    # example of the usage of the function, a piece of C code;
    # if present, it overrides the protocol example
    Optional("example"): str,
}])

schema.validate(fxs)
