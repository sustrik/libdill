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
    "name": str,
    Optional("section"): str,
    "info": str,
    Optional("is_in_libdillimpl"): bool,
    "args": [{
        "name": str,
        "type": str,
        "info": str,
        Optional("suffix"): str,
    }],
    Optional("result"): {
        "type": str,
        Optional("success"): str,
        Optional("error"): str,
        Optional("info"): str,
    },
    "prologue": str,
    Optional("epilogue"): str,
    Optional("protocol"): {
        "section": str,
        "type": str,
        "info": str,
        "example": str,
        Optional("experimental"): bool,
    },
    Optional("add_to_synopsis"): str,
    Optional("add_to_errors"): str,
    Optional("has_handle_argument"): bool,
    Optional("allocates_handle"): bool,
    Optional("has_handle_argument"): bool,
    Optional("has_deadline"): bool,
    Optional("uses_connection"): bool,
    Optional("has_iol"): bool,
    Optional("mem"): str,
    Optional("errors"): [str],
    Optional("custom_errors"): {str: str},
    Optional("example"): str,
}])

schema.validate(fxs)
