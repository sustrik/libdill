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
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cr.h"
#include "libdill.h"
#include "utils.h"

struct dill_proc {
    pid_t pid;
    /* File descriptor to signal when we want the child process to exit.
       In case of parent failure it's signaled with ERR automatically when
       the fd is closed by the OS. */
    int closepipe;
};

/******************************************************************************/
/*  Handle implementation.                                                    */
/******************************************************************************/

static const int dill_proc_type_placeholder = 0;
static const void *dill_proc_type = &dill_proc_type_placeholder;
static int dill_proc_finish(int h, int64_t deadline);
static void dill_proc_close(int h);
static const struct hvfptrs dill_proc_vfptrs = {
    dill_proc_finish,
    dill_proc_close
};

/******************************************************************************/
/*  Creating and terminating processes.                                       */
/******************************************************************************/

int dill_proc_prologue(int *hndl) {
    int err;
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) {err = ECANCELED; goto error1;}
    struct dill_proc *proc = malloc(sizeof(struct dill_proc));
    if(dill_slow(!proc)) {err = ENOMEM; goto error1;}
    proc->pid = -1;
    int closepipe[2];
    rc = pipe(closepipe);
    if(dill_slow(rc < 0)) {err = ENOMEM; goto error2;}
    proc->closepipe = closepipe[1];
    int h = handle(dill_proc_type, proc, &dill_proc_vfptrs);
    if(dill_slow(h < 0)) {err = errno; goto error3;}
    pid_t pid = fork();
    if(dill_slow(pid < 0)) {err = ENOMEM; goto error4;}
    /* Child. */
    if(pid == 0) {
        /* We don't need the sending end of the pipe. */
        close(closepipe[1]);
        /* This call will also promote currently running coroutine to the
           position of main coroutine in the process. */
        dill_postfork(closepipe[0]);
        return 1;
    }
    /* Parent. */
    close(closepipe[0]);
    proc->pid = pid;
    *hndl = h;
    return 0;
error4:
    hclose(h);
error3:
    close(closepipe[0]);
    close(closepipe[1]);
error2:
    free(proc);
error1:
    errno = err;
    *hndl = -1;
    return 0;
}

void dill_proc_epilogue(void) {
    /* Terminate the process. */
    exit(0);
}

static int dill_proc_finish(int h, int64_t deadline) {
    dill_proc_close(h);
    return 0;
}

static void dill_proc_close(int h) {
    struct dill_proc *proc = hdata(h, dill_proc_type);
    dill_assert(proc);
    /* This may happen if forking failed. */
    if(dill_slow(proc->pid < 0)) {free(proc); return;}
    /* There is a child running. Let's send it a kill signal. */
    int rc = close(proc->closepipe);
    dill_assert(rc == 0);
    /* Wait till it finishes. */
    /* TODO: For how long can this block? */
    rc = waitpid(proc->pid, NULL, 0);
    dill_assert(rc >= 0);
    free(proc);
}

