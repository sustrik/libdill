/*
 * Copyright 2019, Mokshasoft AB (mokshasoft.com)
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 3. Note that NO WARRANTY is provided.
 * See "LICENSE.txt" for details.
 */

#include "test-ffi.h"
#include "libdill.h"
#include "tests/assert.h"

coroutine void sender(int ch, int val) {
    int rc = chsend(ch, &val, sizeof(val), -1);
    errno_assert(rc == 0);
}

int ffi_go_sender(int ch, int val) {
    return go(sender(ch, val));
}

coroutine void sender_unblocked(int ch, int val) {
    int rc = chsend(ch, &val, sizeof(val), -1);
    errno_assert(rc == -1 && errno == EPIPE);
}

int ffi_go_sender_unblocked(int ch, int val) {
    return go(sender_unblocked(ch, val));
}

coroutine void receiver(int ch, int expected) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == expected);
}

int ffi_go_receiver(int ch, int expected) {
    return go(receiver(ch, expected));
}
