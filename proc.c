/*

  Copyright (c) 2016 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "libdill.h"
#include "poller.h"
#include "utils.h"

static const int dill_proc_type_placeholder = 0;
static const void *dill_proc_type = &dill_proc_type_placeholder;

struct dill_proc {
    pid_t pid;
};

/* TODO: At the moment hwait() doesn't work for processes. Either call
         waitpid() periodically, or create a pipe to send the signal. */

static void dill_proc_stop_fn(int h) {
    struct dill_proc *proc = hdata(h, dill_proc_type);
    dill_assert(proc);
    /* This may happen if forking failed. */
    if(dill_slow(proc->pid < 0)) goto error1;
    /* There is a child running. Let's send it a kill signal. */
    int rc = kill(proc->pid, SIGKILL);
    dill_assert(rc == 0);
    /* Wait till it finishes. */
    /* TODO: For how long can this block? */
    rc = waitpid(proc->pid, NULL, 0);
    dill_assert(rc >= 0);
error1:
    rc = hdone(h, 0);
    dill_assert(rc >= 0);
    free(proc);
}

int dill_fork_prologue(int *hndl, const char *created) {
    struct dill_proc *proc = malloc(sizeof(struct dill_proc));
    if(dill_slow(!proc)) {errno = ENOMEM; *hndl = -1; return 0;}
    proc->pid = -1;
    int h = handle(dill_proc_type, proc, dill_proc_stop_fn);
    if(dill_slow(h < 0)) {
        int err = errno;
        free(proc);
        errno = err;
        *hndl = -1;
        return 0;
    }
    pid_t pid = dill_fork();
    if(dill_slow(pid < 0)) {
        int err = errno;
        hclose(h);
        errno = err;
        *hndl = -1;
        return 0;
    }
    /* Child. */
    if(pid == 0) return 1;
    /* Parent. */
    proc->pid = pid;
    *hndl = h;
    return 0;
}

void dill_fork_epilogue(int result) {
    exit(result);
}

