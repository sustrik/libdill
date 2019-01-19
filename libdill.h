/*

  Copyright (c) 2018 Martin Sustrik

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
#define DILL_VERSION_CURRENT 24

/*  The latest revision of the current interface. */
#define DILL_VERSION_REVISION 0

/*  How many past interface versions are still supported. */
#define DILL_VERSION_AGE 4

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

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

DILL_EXPORT int dill_fdclean(int fd);
DILL_EXPORT int dill_fdin(int fd, int64_t deadline);
DILL_EXPORT int dill_fdout(int fd, int64_t deadline);
DILL_EXPORT int64_t dill_now(void);
DILL_EXPORT int dill_msleep(int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES
#define fdclean dill_fdclean
#define fdin dill_fdin
#define fdout dill_fdout
#define now dill_now
#define msleep dill_msleep
#endif

/******************************************************************************/
/*  Handles                                                                   */
/******************************************************************************/

DILL_EXPORT int dill_hclose(int h);

#if !defined DILL_DISABLE_RAW_NAMES
#define hclose dill_hclose
#endif

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

#define dill_coroutine __attribute__((noinline))

struct dill_coroutine_opts {
    void *stack;
    size_t stacklen;
};

DILL_EXPORT extern const struct dill_coroutine_opts dill_coroutine_defaults;

DILL_EXPORT extern volatile void *dill_unoptimisable;

DILL_EXPORT __attribute__((noinline)) int dill_prologue(sigjmp_buf **ctx,
    void **ptr, size_t len, int bndl, const char *file, int line);
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
#define dill_setsp(x) \
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
#define dill_setsp(x) \
    asm(""::"r"(alloca(sizeof(size_t))));\
    asm volatile("leal (%%eax), %%esp"::"eax"(x));

/* Stack-switching on other microarchitectures. */
#else
#define dill_setjmp(ctx) sigsetjmp(ctx, 0)
#define dill_longjmp(ctx) siglongjmp(ctx, 1)
/* For newer GCCs, -fstack-protector breaks on this; use -fno-stack-protector.
   Alternatively, implement a custom dill_setsp for your microarchitecture. */
#define dill_setsp(x) \
    dill_unoptimisable = alloca((char*)alloca(sizeof(size_t)) - (char*)(x));
#endif

/* Statement expressions are a gcc-ism but they are also supported by clang.
   Given that there's no other way to do this, screw other compilers for now.
   See https://gcc.gnu.org/onlinedocs/gcc-3.2/gcc/Statement-Exprs.html */

/* A bug in gcc have been observed where name clash between variable in the
   outer scope and a local variable in this macro causes the variable to
   get weird values. To avoid that, we use fancy names (dill_*__). */ 

#define dill_go_(fn, opts, bndl) \
    ({\
        sigjmp_buf *dill_ctx__;\
        void *dill_stk__ = (opts) ? (opts)->stack : NULL;\
        int dill_handle__ = dill_prologue(&dill_ctx__, \
            &dill_stk__, (opts) ? (opts)->stacklen : 0,\
            (bndl), __FILE__, __LINE__);\
        if(dill_handle__ >= 0) {\
            if(!dill_setjmp(*dill_ctx__)) {\
                dill_setsp(dill_stk__);\
                fn;\
                dill_epilogue();\
            }\
        }\
        dill_handle__;\
    })

#define dill_go(fn) dill_go_(fn, ((struct dill_coroutine_opts*)NULL), -1)
#define dill_go_opts(fn, opts) dill_go_(fn, opts, -1)

#define dill_bundle_go(bndl, fn) dill_go_(fn, \
    ((struct dill_coroutine_opts*)NULL), bndl)
#define dill_bundle_go_opts(bndl, fn, opts) dill_go_(fn, opts, bndl)

struct dill_bundle_storage {uint8_t _[64];};

struct dill_bundle_opts {
    struct dill_bundle_storage *mem;
};

DILL_EXPORT extern const struct dill_bundle_opts dill_bundle_defaults;

DILL_EXPORT int dill_bundle(const struct dill_bundle_opts *opts);
DILL_EXPORT int dill_bundle_wait(int h, int64_t deadline);
DILL_EXPORT int dill_yield(void);

#if !defined DILL_DISABLE_RAW_NAMES
#define coroutine dill_coroutine
#define coroutine_opts dill_coroutine_opts
#define coroutine_defaults dill_coroutine_defaults
#define go dill_go
#define go_opts dill_go_opts
#define bundle_go dill_bundle_go
#define bundle_go_opts dill_bundle_go_opts
#define bundle_storage dill_bundle_storage
#define bundle_opts dill_bundle_opts
#define bundle_defaults dill_bundle_defaults
#define bundle dill_bundle
#define bundle_wait dill_bundle_wait
#define yield dill_yield
#endif

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

#define DILL_CHSEND 1
#define DILL_CHRECV 2

struct dill_chclause {
    int op;
    int ch;
    void *val;
    size_t len;
};

struct dill_chstorage {uint8_t _[144];};

struct dill_chopts {
    struct dill_chstorage *mem;
};

DILL_EXPORT extern const struct dill_chopts dill_chdefaults;

DILL_EXPORT int dill_chmake(
    int s[2],
    const struct dill_chopts *opts);
DILL_EXPORT int dill_chsend(
    int ch,
    const void *val,
    size_t len,
    int64_t deadline);
DILL_EXPORT int dill_chrecv(
    int ch,
    void *val,
    size_t len,
    int64_t deadline);
DILL_EXPORT int dill_chdone(
    int ch);
DILL_EXPORT int dill_choose(
    struct dill_chclause *clauses,
    int nclauses,
    int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES
#define CHSEND DILL_CHSEND
#define CHRECV DILL_CHRECV
#define chclause dill_chclause
#define chstorage dill_chstorage
#define chopts dill_chopts
#define chdefaults dill_chdefaults
#define chmake dill_chmake
#define chsend dill_chsend
#define chrecv dill_chrecv
#define chdone dill_chdone
#define choose dill_choose
#endif

#if !defined DILL_DISABLE_SOCKETS

/******************************************************************************/
/*  Gather/scatter list.                                                      */
/******************************************************************************/

struct dill_iolist {
    void *iol_base;
    size_t iol_len;
    struct dill_iolist *iol_next;
    int iol_rsvd;
};

#if !defined DILL_DISABLE_RAW_NAMES
#define iolist dill_iolist
#endif

/******************************************************************************/
/*  Bytestream sockets.                                                       */
/******************************************************************************/

DILL_EXPORT int dill_bsend(
    int s,
    const void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT int dill_brecv(
    int s,
    void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT int dill_bsendl(
    int s,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline);
DILL_EXPORT int dill_brecvl(
    int s,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES
#define bsend dill_bsend
#define brecv dill_brecv
#define bsendl dill_bsendl
#define brecvl dill_brecvl
#endif

/******************************************************************************/
/*  Message sockets.                                                          */
/******************************************************************************/

DILL_EXPORT int dill_msend(
    int s,
    const void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT ssize_t dill_mrecv(
    int s,
    void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT int dill_msendl(
    int s,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline);
DILL_EXPORT ssize_t dill_mrecvl(
    int s,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES
#define msend dill_msend
#define mrecv dill_mrecv
#define msendl dill_msendl
#define mrecvl dill_mrecvl
#endif

/******************************************************************************/
/*  IP address resolution.                                                    */
/******************************************************************************/

struct sockaddr;

#define DILL_IPADDR_IPV4 1
#define DILL_IPADDR_IPV6 2
#define DILL_IPADDR_PREF_IPV4 3
#define DILL_IPADDR_PREF_IPV6 4
#define DILL_IPADDR_MAXSTRLEN 46

struct dill_ipaddr_opts {
    int mode;
};

DILL_EXPORT extern const struct dill_ipaddr_opts dill_ipaddr_defaults;

struct dill_ipaddr {uint8_t _[32];};

DILL_EXPORT int dill_ipaddr_local(
    struct dill_ipaddr *addr,
    const char *name,
    int port,
    const struct dill_ipaddr_opts *opts);
DILL_EXPORT int dill_ipaddr_remote(
    struct dill_ipaddr *addr,
    const char *name,
    int port,
    const struct dill_ipaddr_opts *opts,
    int64_t deadline);
DILL_EXPORT int dill_ipaddr_remotes(
    struct dill_ipaddr *addrs,
    int naddrs,
    const char *name,
    int port,
    const struct dill_ipaddr_opts *opts,
    int64_t deadline);
DILL_EXPORT const char *dill_ipaddr_str(
    const struct dill_ipaddr *addr,
    char *ipstr);
DILL_EXPORT int dill_ipaddr_family(
    const struct dill_ipaddr *addr);
DILL_EXPORT const struct sockaddr *dill_ipaddr_sockaddr(
    const struct dill_ipaddr *addr);
DILL_EXPORT int dill_ipaddr_len(
    const struct dill_ipaddr *addr);
DILL_EXPORT int dill_ipaddr_port(
    const struct dill_ipaddr *addr);
DILL_EXPORT void dill_ipaddr_setport(
    struct dill_ipaddr *addr,
    int port);
DILL_EXPORT int dill_ipaddr_equal(
    const struct dill_ipaddr *addr1,
    const struct dill_ipaddr *addr2,
    int ignore_port);

#if !defined DILL_DISABLE_RAW_NAMES
#define IPADDR_IPV4 DILL_IPADDR_IPV4 
#define IPADDR_IPV6 DILL_IPADDR_IPV6
#define IPADDR_PREF_IPV4 DILL_IPADDR_PREF_IPV4 
#define IPADDR_PREF_IPV6 DILL_IPADDR_PREF_IPV6
#define IPADDR_MAXSTRLEN DILL_IPADDR_MAXSTRLEN
#define ipaddr_opts dill_ipaddr_opts
#define ipaddr_defaults dill_ipaddr_defaults
#define ipaddr dill_ipaddr
#define ipaddr_local dill_ipaddr_local
#define ipaddr_remote dill_ipaddr_remote
#define ipaddr_remotes dill_ipaddr_remotes
#define ipaddr_str dill_ipaddr_str
#define ipaddr_family dill_ipaddr_family
#define ipaddr_sockaddr dill_ipaddr_sockaddr
#define ipaddr_len dill_ipaddr_len
#define ipaddr_port dill_ipaddr_port
#define ipaddr_setport dill_ipaddr_setport
#define ipaddr_equal dill_ipaddr_equal
#endif

/******************************************************************************/
/*  TCP listener.                                                             */
/******************************************************************************/

struct dill_tcp_listener_storage {uint8_t _[40];};

struct dill_tcp_listener_opts {
    struct dill_tcp_listener_storage *mem;
    int backlog;
};

DILL_EXPORT extern const struct dill_tcp_listener_opts
    dill_tcp_listener_defaults;

DILL_EXPORT int dill_tcp_listener_make(
    struct dill_ipaddr *addr,
    const struct dill_tcp_listener_opts *opts);
DILL_EXPORT int dill_tcp_listener_acceptfd(
    int s,
    struct dill_ipaddr *addr,
    int64_t deadline);
DILL_EXPORT int dill_tcp_listener_fromfd(
    int fd,
    const struct dill_tcp_listener_opts *opts);
DILL_EXPORT int dill_tcp_tofd(
    int s);

#define dill_tcp_listen dill_tcp_listener_make

#if !defined DILL_DISABLE_RAW_NAMES
#define tcp_listener_storage dill_tcp_listener_storage
#define tcp_listener_opts dill_tcp_listener_opts
#define tcp_listener_defaults dill_tcp_listener_defaults
#define tcp_listener_make dill_tcp_listener_make
#define tcp_listener_acceptfd dill_tcp_listener_acceptfd
#define tcp_listener_fromfd dill_tcp_listener_fromfd
#define tcp_listener_tofd dill_tcp_listener_tofd
#define tcp_listen dill_tcp_listen
#endif

/******************************************************************************/
/*  TCP connection.                                                           */
/******************************************************************************/

struct dill_tcp_storage {uint8_t _[1];};

struct dill_tcp_opts {
    struct dill_tcp_storage *mem;
    size_t rx_buffer;
    unsigned int nodelay : 1;
};

DILL_EXPORT extern const struct dill_tcp_opts dill_tcp_defaults;

DILL_EXPORT int dill_tcp_accept(
    int s,
    const struct dill_tcp_opts *opts,
    struct dill_ipaddr *addr,
    int64_t deadline);
DILL_EXPORT int dill_tcp_connect(
    const struct dill_ipaddr *addr,
    const struct dill_tcp_opts *opts,
    int64_t deadline);
DILL_EXPORT int dill_tcp_connect_he(
    const char *name,
    int port,
    const struct dill_tcp_opts *opts,
    int64_t deadline);
DILL_EXPORT int dill_tcp_done(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_tcp_close(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_tcp_fromfd(
    int fd,
    const struct dill_tcp_opts *opts);
DILL_EXPORT int dill_tcp_tofd(
    int s);

#if !defined DILL_DISABLE_RAW_NAMES
#define tcp_storage dill_tcp_storage
#define tcp_opts dill_tcp_opts
#define tcp_defaults dill_tcp_defaults
#define tcp_accept dill_tcp_accept
#define tcp_connect dill_tcp_connect
#define tcp_connect_he dill_tcp_connect_he
#define tcp_done dill_tcp_done
#define tcp_close dill_tcp_close
#define tcp_fromfd dill_tcp_fromfd
#define tcp_tofd dill_tcp_tofd
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif

