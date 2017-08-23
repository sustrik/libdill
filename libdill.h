/*

  Copyright (c) 2017 Martin Sustrik

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

#ifndef LIBDILL_H_INCLUDED
#define LIBDILL_H_INCLUDED

#include <errno.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#if defined __linux__
#include <alloca.h>
#endif

/******************************************************************************/
/*  ABI versioning support                                                    */
/******************************************************************************/

/*  Don't change this unless you know exactly what you're doing and have      */
/*  read and understood the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define DILL_VERSION_CURRENT 14

/*  The latest revision of the current interface. */
#define DILL_VERSION_REVISION 0

/*  How many past interface versions are still supported. */
#define DILL_VERSION_AGE 1

/******************************************************************************/
/*  Symbol visibility                                                         */
/******************************************************************************/

#if !defined __GNUC__ && !defined __clang__
#error "Unsupported compiler!"
#endif

#if DILL_NO_EXPORTS
#define DILL_EXPORT
#else
#define DILL_EXPORT __attribute__ ((visibility("default")))
#endif

/* Old versions of GCC don't support visibility attribute. */
#if defined __GNUC__ && __GNUC__ < 4
#undef DILL_EXPORT
#define DILL_EXPORT
#endif

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

DILL_EXPORT int64_t now(void);

/******************************************************************************/
/*  Handles                                                                   */
/******************************************************************************/

DILL_EXPORT int hdup(int h);
DILL_EXPORT int hclose(int h);
DILL_EXPORT int hdone(int h, int64_t deadline);

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

#define coroutine __attribute__((noinline))

DILL_EXPORT extern volatile void *dill_unoptimisable;

DILL_EXPORT __attribute__((noinline)) int dill_prologue(sigjmp_buf **ctx,
    void **ptr, size_t len, const char *file, int line);
DILL_EXPORT __attribute__((noinline)) void dill_epilogue(void);

/* The following macros use alloca(sizeof(size_t)) because clang
   doesn't support alloca with size zero. */

/* This assembly setjmp/longjmp mechanism is in the same order as glibc and
   musl, but glibc implements pointer mangling, which is hard to support.
   This should be binary-compatible with musl, though. */

/* Stack-switching on X86-64. */
#if defined(__x86_64__) && !defined DILL_ARCH_FALLBACK
#define dill_setjmp(ctx) ({\
    int ret;\
    asm("lea     LJMPRET%=(%%rip), %%rcx\n\t"\
        "xor     %%rax, %%rax\n\t"\
        "mov     %%rbx, (%%rdx)\n\t"\
        "mov     %%rbp, 8(%%rdx)\n\t"\
        "mov     %%r12, 16(%%rdx)\n\t"\
        "mov     %%r13, 24(%%rdx)\n\t"\
        "mov     %%r14, 32(%%rdx)\n\t"\
        "mov     %%r15, 40(%%rdx)\n\t"\
        "mov     %%rsp, 48(%%rdx)\n\t"\
        "mov     %%rcx, 56(%%rdx)\n\t"\
        "LJMPRET%=:\n\t"\
        : "=a" (ret)\
        : "d" (ctx)\
        : "memory", "rcx", "rsi", "rdi", "r8", "r9", "r10", "r11", "cc");\
    ret;\
})
#define dill_longjmp(ctx) \
    asm("movq   56(%%rdx), %%rcx\n\t"\
        "movq   48(%%rdx), %%rsp\n\t"\
        "movq   40(%%rdx), %%r15\n\t"\
        "movq   32(%%rdx), %%r14\n\t"\
        "movq   24(%%rdx), %%r13\n\t"\
        "movq   16(%%rdx), %%r12\n\t"\
        "movq   8(%%rdx), %%rbp\n\t"\
        "movq   (%%rdx), %%rbx\n\t"\
        ".cfi_def_cfa %%rdx, 0 \n\t"\
        ".cfi_offset %%rbx, 0 \n\t"\
        ".cfi_offset %%rbp, 8 \n\t"\
        ".cfi_offset %%r12, 16 \n\t"\
        ".cfi_offset %%r13, 24 \n\t"\
        ".cfi_offset %%r14, 32 \n\t"\
        ".cfi_offset %%r15, 40 \n\t"\
        ".cfi_offset %%rsp, 48 \n\t"\
        ".cfi_offset %%rip, 56 \n\t"\
        "jmp    *%%rcx\n\t"\
        : : "d" (ctx), "a" (1))
#define DILL_SETSP(x) \
    asm(""::"r"(alloca(sizeof(size_t))));\
    asm volatile("leaq (%%rax), %%rsp"::"rax"(x));

/* Stack switching on X86. */
#elif defined(__i386__) && !defined DILL_ARCH_FALLBACK
#define dill_setjmp(ctx) ({\
    int ret;\
    asm("movl   $LJMPRET%=, %%ecx\n\t"\
        "movl   %%ebx, (%%edx)\n\t"\
        "movl   %%esi, 4(%%edx)\n\t"\
        "movl   %%edi, 8(%%edx)\n\t"\
        "movl   %%ebp, 12(%%edx)\n\t"\
        "movl   %%esp, 16(%%edx)\n\t"\
        "movl   %%ecx, 20(%%edx)\n\t"\
        "xorl   %%eax, %%eax\n\t"\
        "LJMPRET%=:\n\t"\
        : "=a" (ret) : "d" (ctx) : "memory");\
    ret;\
})
#define dill_longjmp(ctx) \
    asm("movl   (%%edx), %%ebx\n\t"\
        "movl   4(%%edx), %%esi\n\t"\
        "movl   8(%%edx), %%edi\n\t"\
        "movl   12(%%edx), %%ebp\n\t"\
        "movl   16(%%edx), %%esp\n\t"\
        "movl   20(%%edx), %%ecx\n\t"\
        ".cfi_def_cfa %%edx, 0 \n\t"\
        ".cfi_offset %%ebx, 0 \n\t"\
        ".cfi_offset %%esi, 4 \n\t"\
        ".cfi_offset %%edi, 8 \n\t"\
        ".cfi_offset %%ebp, 12 \n\t"\
        ".cfi_offset %%esp, 16 \n\t"\
        ".cfi_offset %%eip, 20 \n\t"\
        "jmp    *%%ecx\n\t"\
        : : "d" (ctx), "a" (1))
#define DILL_SETSP(x) \
    asm(""::"r"(alloca(sizeof(size_t))));\
    asm volatile("leal (%%eax), %%esp"::"eax"(x));

/* Stack-switching on other microarchitectures. */
#else
#define dill_setjmp(ctx) sigsetjmp(ctx, 0)
#define dill_longjmp(ctx) siglongjmp(ctx, 1)
/* For newer GCCs, -fstack-protector breaks on this; use -fno-stack-protector.
   Alternatively, implement a custom DILL_SETSP for your microarchitecture. */
#define DILL_SETSP(x) \
    dill_unoptimisable = alloca((char*)alloca(sizeof(size_t)) - (char*)(x));
#endif

/* Statement expressions are a gcc-ism but they are also supported by clang.
   Given that there's no other way to do this, screw other compilers for now.
   See https://gcc.gnu.org/onlinedocs/gcc-3.2/gcc/Statement-Exprs.html */

/* A bug in gcc have been observed where name clash between variable in the
   outer scope and a local variable in this macro causes the variable to
   get weird values. To avoid that, we use fancy names (dill_*__). */ 

#define go_mem(fn, ptr, len) \
    ({\
        sigjmp_buf *dill_ctx__;\
        void *dill_stk__ = (ptr);\
        int dill_handle__ = dill_prologue(&dill_ctx__, &dill_stk__, (len),\
            __FILE__, __LINE__);\
        if(dill_handle__ >= 0) {\
            if(!dill_setjmp(*dill_ctx__)) {\
                DILL_SETSP(dill_stk__);\
                fn;\
                dill_epilogue();\
            }\
        }\
        dill_handle__;\
    })

#define go(fn) go_mem(fn, NULL, 0)

DILL_EXPORT int co(void **ptr, size_t len,
    void *fn, const char *file, int line,
    void (*routine)(void *));
DILL_EXPORT int yield(void);
DILL_EXPORT int msleep(int64_t deadline);
DILL_EXPORT int fdclean(int fd);
DILL_EXPORT int fdin(int fd, int64_t deadline);
DILL_EXPORT int fdout(int fd, int64_t deadline);

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

#define CHSEND 1
#define CHRECV 2

struct chclause {
    int op;
    int ch;
    void *val;
    size_t len;
};

struct chmem {
#if defined(__i386__)
    char reserved[40];
#else
    char reserved[80];
#endif
};

DILL_EXPORT int chmake(size_t itemsz);
DILL_EXPORT int chmake_mem(size_t itemsz, struct chmem *mem);
DILL_EXPORT int chsend(int ch, const void *val, size_t len, int64_t deadline);
DILL_EXPORT int chrecv(int ch, void *val, size_t len, int64_t deadline);
DILL_EXPORT int choose(struct chclause *clauses, int nclauses,
    int64_t deadline);

#define chdone(ch) hdone((ch), -1)

#if !defined DILL_DISABLE_SOCKETS

/******************************************************************************/
/*  Gather/scatter list.                                                      */
/******************************************************************************/

struct iolist {
    void *iol_base;
    size_t iol_len;
    struct iolist *iol_next;
    int iol_rsvd;
};

/******************************************************************************/
/*  Bytestream sockets.                                                       */
/******************************************************************************/

DILL_EXPORT int bsend(
    int s,
    const void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT ssize_t brecv(
    int s,
    void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT int bsendl(
    int s,
    struct iolist *first,
    struct iolist *last,
    int64_t deadline);
DILL_EXPORT ssize_t brecvl(
    int s,
    struct iolist *first,
    struct iolist *last,
    int64_t deadline);

/******************************************************************************/
/*  Message sockets.                                                          */
/******************************************************************************/

DILL_EXPORT int msend(
    int s,
    const void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT ssize_t mrecv(
    int s,
    void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT int msendl(
    int s,
    struct iolist *first,
    struct iolist *last,
    int64_t deadline);
DILL_EXPORT ssize_t mrecvl(
    int s,
    struct iolist *first,
    struct iolist *last,
    int64_t deadline);

/******************************************************************************/
/*  IP address resolution.                                                    */
/******************************************************************************/

struct sockaddr;

#define IPADDR_IPV4 1
#define IPADDR_IPV6 2
#define IPADDR_PREF_IPV4 3
#define IPADDR_PREF_IPV6 4
#define IPADDR_MAXSTRLEN 46

struct ipaddr {
    char data[32];
};

DILL_EXPORT int ipaddr_local(
    struct ipaddr *addr,
    const char *name,
    int port,
    int mode);
DILL_EXPORT int ipaddr_remote(
    struct ipaddr *addr,
    const char *name,
    int port,
    int mode,
    int64_t deadline);
DILL_EXPORT const char *ipaddr_str(
    const struct ipaddr *addr,
    char *ipstr);
DILL_EXPORT int ipaddr_family(
    const struct ipaddr *addr);
DILL_EXPORT const struct sockaddr *ipaddr_sockaddr(
    const struct ipaddr *addr);
DILL_EXPORT int ipaddr_len(
    const struct ipaddr *addr);
DILL_EXPORT int ipaddr_port(
    const struct ipaddr *addr);
DILL_EXPORT void ipaddr_setport(
    struct ipaddr *addr,
    int port);

/******************************************************************************/
/*  TCP protocol.                                                             */
/******************************************************************************/

DILL_EXPORT int tcp_listen(
    struct ipaddr *addr,
    int backlog);
DILL_EXPORT int tcp_accept(
    int s,
    struct ipaddr *addr,
    int64_t deadline);
DILL_EXPORT int tcp_connect(
    const struct ipaddr *addr,
    int64_t deadline);
DILL_EXPORT int tcp_close(
    int s,
    int64_t deadline);

/******************************************************************************/
/*  IPC protocol.                                                            */
/******************************************************************************/

DILL_EXPORT int ipc_listen(
    const char *addr,
    int backlog);
DILL_EXPORT int ipc_accept(
    int s,
    int64_t deadline);
DILL_EXPORT int ipc_connect(
    const char *addr,
    int64_t deadline);
DILL_EXPORT int ipc_close(
    int s,
    int64_t deadline);
DILL_EXPORT int ipc_pair(
    int s[2]);

/******************************************************************************/
/*  PFX protocol.                                                             */
/*  Messages are prefixed by 8-byte size in network byte order.               */
/*  The protocol is terminated by 0xffffffffffffffff.                         */
/******************************************************************************/

DILL_EXPORT int pfx_attach(
    int s);
DILL_EXPORT int pfx_detach(
    int s,
    int64_t deadline);

/******************************************************************************/
/*  CRLF protocol.                                                            */
/*  Messages are delimited by CRLF (0x0d 0x0a) sequences.                     */
/*  The protocol is terminated by an empty line.                              */
/******************************************************************************/

DILL_EXPORT int crlf_attach(
    int s);
DILL_EXPORT int crlf_detach(
    int s,
    int64_t deadline);

#endif

#endif

