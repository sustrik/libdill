/*
 * Copyright 2019, Mokshasoft AB (mokshasoft.com)
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 3. Note that NO WARRANTY is provided.
 * See "LICENSE.txt" for details.
 */

int ffi_go_sender(int ch, int val);
int ffi_go_sender_unblocked(int ch, int val);
int ffi_go_receiver(int ch, int expected);
