# Copyright (c) 2019 Martin Sustrik  All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom
# the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

from inspect import currentframe
from itertools import dropwhile

def _trim(t1):
    lns = [ln.rstrip() for ln in t1]
    lns = [ln for ln in dropwhile(lambda ln: len(ln) == 0, lns)]
    lns = [ln for ln in dropwhile(lambda ln: len(ln) == 0, reversed(lns))]
    lf  = filter(bool, lns)
    left = 0 if not lf else \
           min([len(ln) - len(ln.lstrip()) for ln in lf], default=0)
    return [ln[left:] for ln in reversed(lns)]

def _append(t1, t2):
    t1 += [""] * (len(t2) - len(t1))
    w = max([len(s) for s in t1 or [""]])
    for i in range(len(t2)):
        t1[i] = t1[i].ljust(w) + t2[i]

def _format(s, g, l):
    lns = s.split("\n")
    res = []
    for ln in lns:
        curr = []; end = 0
        while True:
            start = ln.find("@{", end)
            _append(curr, [ln[end : (len(ln) if start == -1 else start)]])
            if start == -1:
                break
            end = ln.find("}", start) + 1
            if end == 0:
                raise Exception("unifinished @{} expression")
            _append(curr,
                str(eval(ln[start + 2 : end - 1], g, l)).split("\n"))
        res += curr
    return res

class _tile:
   def __init__(self, lns):
       self.lns = lns
   def __truediv__(self, x):
       return _tile(_trim(_format(x,
           currentframe().f_back.f_globals, currentframe().f_back.f_locals)))
   def __mod__(self, x):
       return _tile(_format(x,
           currentframe().f_back.f_globals, currentframe().f_back.f_locals))
   def __or__(self, x):
       if not isinstance(x, _tile):
           raise Exception("argument is not a tile")
       return _tile(self.lns + x.lns)
   def __add__(self, x):
       if not isinstance(x, _tile):
           raise Exception("argument is not a tile")
       lns = self.lns.copy()
       _append(lns, x.lns)
       return _tile(lns)
   def join(self, x, last=None):
       res = t/""
       for idx, item in enumerate(x):
           res += item
           if idx < len(x) - 1:
               res += self
       return res + (t/"" if not last else last)
   def vjoin(self, x, inline=True, last=None):
       if last == None:
           last = t/""
       res = t/""
       for idx, item in enumerate(x):
           res |= item + ((self if idx < len(x) - 1 else last) if inline else t/"") 
           if not inline:
               res |= (self if idx < len(x) - 1 else last) 
       return res
   def __str__(self):
       return "\n".join(self.lns)

t = _tile([])

