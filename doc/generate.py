#!/usr/bin/env python3

import glob

def trimrect(s):
    return ""

# Read all topic files.
fxs = []
files = glob.glob("*.topic.py")
for file in files:
    print(file)
    with open(file, 'r') as f:
        c = f.read()
        exec(c)

# Read all topic files.
fxs = []
files = glob.glob("*.function.py")
for file in files:
    print(file)
    with open(file, 'r') as f:
        c = f.read()
        exec(c)

