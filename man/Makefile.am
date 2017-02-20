#
# Copyright (c) 2016 Martin Sustrik  All rights reserved.
# Copyright (c) 2016 Tai Chi Minh Ralph Eastwood
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

man3_MANS = \
    chmake.3 \
    chmake_mem.3 \
    choose.3 \
    chrecv.3 \
    chsend.3 \
    fdclean.3 \
    fdin.3 \
    fdout.3 \
    go.3 \
    go_mem.3 \
    hclose.3 \
    hdone.3 \
    hdup.3 \
    hmake.3 \
    hquery.3 \
    msleep.3 \
    now.3 \
    yield.3

SUFFIXES = .md .3

man-local: $(man3_MANS)

# Compile the man page document
.md.3:
	$(AM_V_GEN) @PANDOC@ -s -t man $< -o $@ \
		-M author="Martin Sustrik" \
		-M header="libdill Library Functions" \
		-M footer="libdill" \
		-M title="`echo "$(patsubst %.md,%,$<)(3)" | tr 'a-z' 'A-Z'`"

clean-local: clean-local-check

.PHONY: clean-local-check

clean-local-check:
	-rm $(man3_MANS)

EXTRA_DIST = $(man3_MANS)

