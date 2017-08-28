# The MIT License (MIT)
#
# Copyright (c) 2017 Robin Linden <dev@robinlinden.eu>
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
#
# - Try to find libdill
# Once done this will define
#  LIBDILL_FOUND
#  LIBDILL_INCLUDE_DIRS
#  LIBDILL_LIBRARIES

FIND_PATH(LIBDILL_INCLUDE_DIR NAMES libdill.h)
FIND_LIBRARY(LIBDILL_LIBRARY NAMES dill)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(libdill
                                  DEFAULT_MSG
                                  LIBDILL_LIBRARY
                                  LIBDILL_INCLUDE_DIR)

MARK_AS_ADVANCED(LIBDILL_INCLUDE_DIR LIBDILL_LIBRARY)

SET(LIBDILL_INCLUDE_DIRS "${LIBDILL_INCLUDE_DIR}")
SET(LIBDILL_LIBRARIES "${LIBDILL_LIBRARY}")
