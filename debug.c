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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "chan.h"
#include "cr.h"
#include "libdill.h"
#include "list.h"
#include "poller.h"
#include "stack.h"
#include "utils.h"

/* ID to be assigned to next launched coroutine. */
static int dill_next_cr_id = 1;

/* List of all coroutines. */
static struct dill_list dill_all_crs = {
    &dill_main.debug.item, &dill_main.debug.item};

/* ID to be assigned to the next created channel. */
static int dill_next_chan_id = 1;

/* List of all channels. */
static struct dill_list dill_all_chans = {0};

void dill_register_cr(struct dill_debug_cr *cr, const char *created) {
    dill_list_insert(&dill_all_crs, &cr->item, NULL);
    cr->id = dill_next_cr_id;
    ++dill_next_cr_id;
    cr->created = created;
    cr->current = NULL;
}

void dill_unregister_cr(struct dill_debug_cr *cr) {
    dill_list_erase(&dill_all_crs, &cr->item);
}

void dill_register_chan(struct dill_debug_chan *ch, const char *created) {
    dill_list_insert(&dill_all_chans, &ch->item, NULL);
    ch->id = dill_next_chan_id;
    ++dill_next_chan_id;
    ch->created = created;
}

void dill_unregister_chan(struct dill_debug_chan *ch) {
    dill_list_erase(&dill_all_chans, &ch->item);
}

void dill_startop(struct dill_debug_cr *cr, enum dill_op op,
      const char *current) {
    cr->op = op;
    cr->current = current;
}

void goredump(void) {
    char buf[512];
    char idbuf[10];

    fprintf(stderr,
        "\nCOROUTINE  state                                      "
        "current                                  created\n");
    fprintf(stderr,
        "----------------------------------------------------------------------"
        "--------------------------------------------------\n");
    struct dill_list_item *it;
    for(it = dill_list_begin(&dill_all_crs); it; it = dill_list_next(it)) {
        struct dill_cr *cr = dill_cont(it, struct dill_cr, debug.item);
        if(dill_running != cr) {
            switch(cr->debug.op) {
            case DILL_YIELD:
                sprintf(buf, "yield()");
                break;
            case DILL_MSLEEP:
                sprintf(buf, "msleep()");
                break;
            case DILL_FDWAIT:
                {
                    struct dill_fdwaitdata *fdata =
                        (struct dill_fdwaitdata*)cr->opaque;
                    sprintf(buf, "fdwait(%d, %s)", fdata->fd,
                        (fdata->events & FDW_IN) &&
                            (fdata->events & FDW_OUT) ? "FDW_IN | FDW_OUT" :
                        fdata->events & FDW_IN ? "FDW_IN" :
                        fdata->events & FDW_OUT ? "FDW_OUT" : 0);
                    break;
                }
            case DILL_CHSEND:
            case DILL_CHRECV:
            case DILL_CHOOSE:
                {
                    struct dill_choosedata *cd =
                        (struct dill_choosedata*)cr->opaque;
                    int pos = 0;
                    if(cr->debug.op == DILL_CHRECV)
                        pos += sprintf(&buf[pos], "chrecv(");
                    else if(cr->debug.op == DILL_CHSEND)
                        pos += sprintf(&buf[pos], "chsend(");
                    else
                        pos += sprintf(&buf[pos], "choose(");
                    int first = 1;
                    int i;
                    for(i = 0; i != cd->nclauses; ++i) {
                        if(first)
                            first = 0;
                        else
                            pos += sprintf(&buf[pos], ",");
                        if(cr->debug.op == DILL_CHOOSE) {
                            buf[pos] =
                                cd->clauses[i].op == CHSEND ? 'S' : 'R';
                            ++pos;
                        }
                        pos += sprintf(&buf[pos], "<%d>",
                            cd->clauses[i].channel->debug.id);
                    }
                    sprintf(&buf[pos], ")");
                }
                break;
            case DILL_STOP:
                {
                    struct dill_stopdata *sd =
                        (struct dill_stopdata*)cr->opaque;
                    int pos = 0;
                    pos += sprintf(&buf[pos], "stop(");
                    int first = 1;
                    int i;
                    for(i = 0; i != sd->nhndls; ++i) {
                        if(first)
                            first = 0;
                        else
                            pos += sprintf(&buf[pos], ",");
                        pos += sprintf(&buf[pos], "{%d}",
                            sd->hndls[i]->debug.id);
                    }
                    sprintf(&buf[pos], ")");
                }
                break;
            case DILL_FINISHED:
                sprintf(buf, " finished");
                break;
            default:
                assert(0);
            }
        }
        snprintf(idbuf, sizeof(idbuf), "{%d}", (int)cr->debug.id);
        fprintf(stderr, "%-8s   %-42s %-40s %s\n",
            idbuf,
            cr == dill_running ? "RUNNING" : buf,
            cr == dill_running || !cr->debug.current ?
                "---" : cr->debug.current,
            cr->debug.created ? cr->debug.created : "<main>");
    }
    fprintf(stderr,"\n");

    if(dill_list_empty(&dill_all_chans))
        return;
    fprintf(stderr,
        "CHANNEL  msgs/max    senders/receivers                          "
        "refs  done  created\n");
    fprintf(stderr,
        "----------------------------------------------------------------------"
        "--------------------------------------------------\n");
    for(it = dill_list_begin(&dill_all_chans); it; it = dill_list_next(it)) {
        struct dill_chan *ch = dill_cont(it, struct dill_chan, debug.item);
        snprintf(idbuf, sizeof(idbuf), "<%d>", (int)ch->debug.id);
        sprintf(buf, "%d/%d",
            (int)ch->items,
            (int)ch->bufsz);
        fprintf(stderr, "%-8s %-11s ",
            idbuf,
            buf);
        int pos;
        struct dill_list *clauselist;
        if(!dill_list_empty(&ch->sender.clauses)) {
            pos = sprintf(buf, "s:");
            clauselist = &ch->sender.clauses;
        }
        else if(!dill_list_empty(&ch->receiver.clauses)) {
            pos = sprintf(buf, "r:");
            clauselist = &ch->receiver.clauses;
        }
        else {
            sprintf(buf, " ");
            clauselist = NULL;
        }
        struct dill_clause *cl = NULL;
        if(clauselist)
            cl = dill_cont(dill_list_begin(clauselist),
                struct dill_clause, epitem);
        int first = 1;
        while(cl) {
            if(first)
                first = 0;
            else
                pos += sprintf(&buf[pos], ",");
            pos += sprintf(&buf[pos], "{%d}", (int)cl->cr->debug.id);
            cl = dill_cont(dill_list_next(&cl->epitem),
                struct dill_clause, epitem);
        }
        fprintf(stderr, "%-42s %-5d %-5s %s\n",
            buf,
            (int)ch->refcount,
            ch->done ? "yes" : "no",
            ch->debug.created);
    }
    fprintf(stderr,"\n");
}

int dill_tracelevel = 0;

void gotrace(int level) {
    dill_tracelevel = level;
}

void dill_trace_(const char *location, const char *format, ...) {
    if(dill_fast(dill_tracelevel <= 0))
        return;

    char buf[256];

    /* First print the timestamp. */
    struct timeval nw;
    gettimeofday(&nw, NULL);
    struct tm *nwtm = localtime(&nw.tv_sec);
    snprintf(buf, sizeof buf, "%02d:%02d:%02d",
        (int)nwtm->tm_hour, (int)nwtm->tm_min, (int)nwtm->tm_sec);
    fprintf(stderr, "==> %s.%06d ", buf, (int)nw.tv_usec);

    /* Coroutine ID. */
    snprintf(buf, sizeof(buf), "{%d}", (int)dill_running->debug.id);
    fprintf(stderr, "%-8s ", buf);

    va_list va;
    va_start(va ,format);
    vfprintf(stderr, format, va);
    va_end(va);
    if(location)
        fprintf(stderr, " at %s\n", location);
    else
        fprintf(stderr, "\n");
    fflush(stderr);
}

void dill_preserve_debug(void) {
    /* Do nothing, but trick the compiler into thinking that the debug
       functions are being used so that it does not optimise them away. */
    static volatile int unoptimisable = 1;
    if(unoptimisable)
        return;
    goredump();
    gotrace(0);
}

