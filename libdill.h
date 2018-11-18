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

DILL_EXPORT int dill_hown(int h);
DILL_EXPORT int dill_hclose(int h);

#if !defined DILL_DISABLE_RAW_NAMES
#define hown dill_hown
#define hclose dill_hclose
#endif

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

#define dill_coroutine __attribute__((noinline))

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

#define dill_go_(fn, ptr, len, bndl) \
    ({\
        sigjmp_buf *dill_ctx__;\
        void *dill_stk__ = (ptr);\
        int dill_handle__ = dill_prologue(&dill_ctx__, &dill_stk__, (len),\
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

#define dill_go(fn) dill_go_(fn, NULL, 0, -1)
#define dill_go_mem(fn, ptr, len) dill_go_(fn, ptr, len, -1)

#define dill_bundle_go(bndl, fn) dill_go_(fn, NULL, 0, bndl)
#define dill_bundle_go_mem(bndl, fn, ptr, len) dill_go_(fn, ptr, len, bndl)

struct dill_bundle_storage {char _[64];};

DILL_EXPORT int dill_bundle(void);
DILL_EXPORT int dill_bundle_mem(struct dill_bundle_storage *mem);
DILL_EXPORT int dill_bundle_wait(int h, int64_t deadline);
DILL_EXPORT int dill_yield(void);

#if !defined DILL_DISABLE_RAW_NAMES
#define coroutine dill_coroutine
#define go dill_go
#define go_mem dill_go_mem
#define bundle_go dill_bundle_go
#define bundle_go_mem dill_bundle_go_mem
#define bundle_storage dill_bundle_storage
#define bundle dill_bundle
#define bundle_mem dill_bundle_mem
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

struct dill_chstorage {char _[144];};

DILL_EXPORT int dill_chmake(
    int chv[2]);
DILL_EXPORT int dill_chmake_mem(
    struct dill_chstorage *mem,
    int chv[2]);
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
#define chmake dill_chmake
#define chmake_mem dill_chmake_mem
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

struct dill_ipaddr {char _[32];};

DILL_EXPORT int dill_ipaddr_local(
    struct dill_ipaddr *addr,
    const char *name,
    int port,
    int mode);
DILL_EXPORT int dill_ipaddr_remote(
    struct dill_ipaddr *addr,
    const char *name,
    int port,
    int mode,
    int64_t deadline);
DILL_EXPORT int dill_ipaddr_remotes(
    struct dill_ipaddr *addrs,
    int naddrs,
    const char *name,
    int port,
    int mode,
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
/*  TCP protocol.                                                             */
/******************************************************************************/

struct dill_tcp_listener_storage {char _[56];};

struct dill_tcp_storage {char _[72];};

DILL_EXPORT int dill_tcp_listen(
    struct dill_ipaddr *addr,
    int backlog);
DILL_EXPORT int dill_tcp_listen_mem(
    struct dill_ipaddr *addr,
    int backlog,
    struct dill_tcp_listener_storage *mem);
DILL_EXPORT int dill_tcp_accept(
    int s,
    struct dill_ipaddr *addr,
    int64_t deadline);
DILL_EXPORT int dill_tcp_accept_mem(
    int s,
    struct dill_ipaddr *addr,
    struct dill_tcp_storage *mem,
    int64_t deadline);
DILL_EXPORT int dill_tcp_connect(
    const struct dill_ipaddr *addr,
    int64_t deadline);
DILL_EXPORT int dill_tcp_connect_mem(
    const struct dill_ipaddr *addr,
    struct dill_tcp_storage *mem,
    int64_t deadline);
DILL_EXPORT int dill_tcp_done(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_tcp_close(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_tcp_listener_fromfd(
    int fd);
DILL_EXPORT int dill_tcp_listener_fromfd_mem(
    int fd,
    struct dill_tcp_listener_storage *mem);
DILL_EXPORT int dill_tcp_fromfd(
    int fd);
DILL_EXPORT int dill_tcp_fromfd_mem(
    int fd,
    struct dill_tcp_storage *mem);

#if !defined DILL_DISABLE_RAW_NAMES
#define tcp_listener_storage dill_tcp_listener_storage
#define tcp_storage dill_tcp_storage
#define tcp_listen dill_tcp_listen
#define tcp_listen_mem dill_tcp_listen_mem
#define tcp_accept dill_tcp_accept
#define tcp_accept_mem dill_tcp_accept_mem
#define tcp_connect dill_tcp_connect
#define tcp_connect_mem dill_tcp_connect_mem
#define tcp_done dill_tcp_done
#define tcp_close dill_tcp_close
#define tcp_listener_fromfd dill_tcp_listener_fromfd
#define tcp_listener_fromfd_mem dill_tcp_listener_fromfd_mem
#define tcp_fromfd dill_tcp_fromfd
#define tcp_fromfd_mem dill_tcp_fromfd_mem
#endif

/******************************************************************************/
/*  IPC protocol.                                                            */
/******************************************************************************/

struct dill_ipc_listener_storage {char _[24];};

struct dill_ipc_storage {char _[72];};

struct dill_ipc_pair_storage {char _[144];};

DILL_EXPORT int dill_ipc_listen(
    const char *addr,
    int backlog);
DILL_EXPORT int dill_ipc_listen_mem(
    const char *addr,
    int backlog,
    struct dill_ipc_listener_storage *mem);
DILL_EXPORT int dill_ipc_accept(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_ipc_accept_mem(
    int s,
    struct dill_ipc_storage *mem,
    int64_t deadline);
DILL_EXPORT int dill_ipc_connect(
    const char *addr,
    int64_t deadline);
DILL_EXPORT int dill_ipc_connect_mem(
    const char *addr,
    struct dill_ipc_storage *mem,
    int64_t deadline);
DILL_EXPORT int dill_ipc_sendfd(
    int s,
    int fd,
    int64_t deadline);
DILL_EXPORT int dill_ipc_recvfd(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_ipc_done(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_ipc_close(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_ipc_listener_fromfd(
    int fd);
DILL_EXPORT int dill_ipc_listener_fromfd_mem(
    int fd,
    struct dill_ipc_listener_storage *mem);
DILL_EXPORT int dill_ipc_fromfd(
    int fd);
DILL_EXPORT int dill_ipc_fromfd_mem(
    int fd,
    struct dill_ipc_storage *mem);
DILL_EXPORT int dill_ipc_pair(
    int s[2]);
DILL_EXPORT int dill_ipc_pair_mem(
    struct dill_ipc_pair_storage *mem,
    int s[2]);

#if !defined DILL_DISABLE_RAW_NAMES
#define ipc_listener_storage dill_ipc_listener_storage
#define ipc_storage dill_ipc_storage
#define ipc_pair_storage dill_ipc_pair_storage
#define ipc_listen dill_ipc_listen
#define ipc_listen_mem dill_ipc_listen_mem
#define ipc_accept dill_ipc_accept
#define ipc_accept_mem dill_ipc_accept_mem
#define ipc_connect dill_ipc_connect
#define ipc_connect_mem dill_ipc_connect_mem
#define ipc_sendfd dill_ipc_sendfd
#define ipc_recvfd dill_ipc_recvfd
#define ipc_done dill_ipc_done
#define ipc_close dill_ipc_close
#define ipc_listener_fromfd dill_ipc_listener_fromfd
#define ipc_listener_fromfd_mem dill_ipc_listener_fromfd_mem
#define ipc_fromfd dill_ipc_fromfd
#define ipc_fromfd_mem dill_ipc_fromfd_mem
#define ipc_pair dill_ipc_pair
#define ipc_pair_mem dill_ipc_pair_mem
#endif

/******************************************************************************/
/*  PREFIX protocol.                                                          */
/*  Messages are prefixed by size.                                            */
/******************************************************************************/

struct dill_prefix_storage {char _[56];};

#define DILL_PREFIX_BIG_ENDIAN 0
#define DILL_PREFIX_LITTLE_ENDIAN 1

DILL_EXPORT int dill_prefix_attach(
    int s,
    size_t hdrlen,
    int flags);
DILL_EXPORT int dill_prefix_attach_mem(
    int s,
    size_t hdrlen,
    int flags,
    struct dill_prefix_storage *mem);
DILL_EXPORT int dill_prefix_detach(
    int s);

#if !defined DILL_DISABLE_RAW_NAMES
#define PREFIX_BIG_ENDIAN DILL_PREFIX_BIG_ENDIAN
#define PREFIX_LITTLE_ENDIAN DILL_PREFIX_LITTLE_ENDIAN
#define prefix_storage dill_prefix_storage
#define prefix_attach dill_prefix_attach
#define prefix_attach_mem dill_prefix_attach_mem
#define prefix_detach dill_prefix_detach
#endif

/******************************************************************************/
/*  SUFFIX protocol.                                                          */
/*  Messages are suffixed by specified string of bytes.                       */
/******************************************************************************/

struct dill_suffix_storage {char _[128];};

DILL_EXPORT int dill_suffix_attach(
    int s,
    const void *suffix,
    size_t suffixlen);
DILL_EXPORT int dill_suffix_attach_mem(
    int s,
    const void *suffix,
    size_t suffixlen,
    struct dill_suffix_storage *mem);
DILL_EXPORT int dill_suffix_detach(
    int s,
    int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES
#define suffix_storage dill_suffix_storage
#define suffix_attach dill_suffix_attach
#define suffix_attach_mem dill_suffix_attach_mem
#define suffix_detach dill_suffix_detach
#endif

/******************************************************************************/
/*  UDP protocol.                                                             */
/*  Each UDP packet is treated as a separate message.                         */
/******************************************************************************/

struct dill_udp_storage {char _[72];};

DILL_EXPORT int dill_udp_open(
    struct dill_ipaddr *local,
    const struct dill_ipaddr *remote);
DILL_EXPORT int dill_udp_open_mem(
    struct dill_ipaddr *local,
    const struct dill_ipaddr *remote,
    struct dill_udp_storage *mem);
DILL_EXPORT int dill_udp_send(
    int s,
    const struct dill_ipaddr *addr,
    const void *buf,
    size_t len);
DILL_EXPORT ssize_t dill_udp_recv(
    int s,
    struct dill_ipaddr *addr,
    void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT int dill_udp_sendl(
    int s,
    const struct dill_ipaddr *addr,
    struct dill_iolist *first,
    struct dill_iolist *last);
DILL_EXPORT ssize_t dill_udp_recvl(
    int s,
    struct dill_ipaddr *addr,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES
#define udp_storage dill_udp_storage
#define udp_open dill_udp_open
#define udp_open_mem dill_udp_open_mem
#define udp_send dill_udp_send
#define udp_recv dill_udp_recv
#define udp_sendl dill_udp_sendl
#define udp_recvl dill_udp_recvl
#endif

/******************************************************************************/
/*  HTTP                                                                      */
/******************************************************************************/

struct dill_http_storage {char _[1296];};

DILL_EXPORT int dill_http_attach(
    int s);
DILL_EXPORT int dill_http_attach_mem(
    int s,
    struct dill_http_storage *mem);
DILL_EXPORT int dill_http_done(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_http_detach(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_http_sendrequest(
    int s,
    const char *command,
    const char *resource,
    int64_t deadline);
DILL_EXPORT int dill_http_recvrequest(
    int s,
    char *command,
    size_t commandlen,
    char *resource,
    size_t resourcelen,
    int64_t deadline);
DILL_EXPORT int dill_http_sendstatus(
    int s,
    int status,
    const char *reason,
    int64_t deadline);
DILL_EXPORT int dill_http_recvstatus(
    int s,
    char *reason,
    size_t reasonlen,
    int64_t deadline);
DILL_EXPORT int dill_http_sendfield(
    int s,
    const char *name,
    const char *value,
    int64_t deadline);
DILL_EXPORT int dill_http_recvfield(
    int s,
    char *name,
    size_t namelen,
    char *value,
    size_t valuelen,
    int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES
#define http_storage dill_http_storage
#define http_attach dill_http_attach
#define http_attach_mem dill_http_attach_mem
#define http_done dill_http_done
#define http_detach dill_http_detach
#define http_sendrequest dill_http_sendrequest
#define http_recvrequest dill_http_recvrequest
#define http_sendstatus dill_http_sendstatus
#define http_recvstatus dill_http_recvstatus
#define http_sendfield dill_http_sendfield
#define http_recvfield dill_http_recvfield
#endif

#if !defined DILL_DISABLE_TLS

/******************************************************************************/
/*  TLS protocol.                                                             */
/******************************************************************************/

struct dill_tls_storage {char _[72];};

DILL_EXPORT int dill_tls_attach_server(
    int s,
    const char *cert,
    const char *pkey,
    int64_t deadline);
DILL_EXPORT int dill_tls_attach_server_mem(
    int s,
    const char *cert,
    const char *pkey,
    struct dill_tls_storage *mem,
    int64_t deadline);
DILL_EXPORT int dill_tls_attach_client(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_tls_attach_client_mem(
    int s,
    struct dill_tls_storage *mem,
    int64_t deadline);
DILL_EXPORT int dill_tls_done(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_tls_detach(
    int s,
    int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES
#define tls_storage dill_tls_storage
#define tls_attach_server dill_tls_attach_server
#define tls_attach_server_mem dill_tls_attach_server_mem
#define tls_attach_client dill_tls_attach_client
#define tls_attach_client_mem dill_tls_attach_client_mem
#define tls_done dill_tls_done
#define tls_detach dill_tls_detach
#endif

/******************************************************************************/
/*  DTLS protocol.                                                            */
/******************************************************************************/

struct dill_dtls_storage {char _[88];};

DILL_EXPORT int dill_dtls_attach_server(
    int s,
    const char *cert,
    const char *pkey,
    int64_t deadline);
DILL_EXPORT int dill_dtls_attach_server_mem(
    int s,
    const char *cert,
    const char *pkey,
    struct dill_dtls_storage *mem,
    int64_t deadline);
DILL_EXPORT int dill_dtls_attach_client(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_dtls_attach_client_mem(
    int s,
    struct dill_dtls_storage *mem,
    int64_t deadline);
DILL_EXPORT int dill_dtls_done(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_dtls_detach(
    int s,
    int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES
#define dtls_storage dill_dtls_storage
#define dtls_attach_server dill_dtls_attach_server
#define dtls_attach_server_mem dill_dtls_attach_server_mem
#define dtls_attach_client dill_dtls_attach_client
#define dtls_attach_client_mem dill_dtls_attach_client_mem
#define dtls_done dill_dtls_done
#define dtls_detach dill_dtls_detach
#endif

#endif

/******************************************************************************/
/*  WebSockets protocol.                                                      */
/******************************************************************************/

struct dill_ws_storage {char _[176];};

#define DILL_WS_BINARY 0
#define DILL_WS_TEXT 1
#define DILL_WS_NOHTTP 2

DILL_EXPORT int dill_ws_attach_client(
    int s,
    int flags,
    const char *resource,
    const char *host,
    int64_t deadline);
DILL_EXPORT int dill_ws_attach_client_mem(
    int s,
    int flags,
    const char *resource,
    const char *host,
    struct dill_ws_storage *mem,
    int64_t deadline);
DILL_EXPORT int dill_ws_attach_server(
    int s,
    int flags,
    char *resource,
    size_t resourcelen,
    char *host,
    size_t hostlen,
    int64_t deadline);
DILL_EXPORT int dill_ws_attach_server_mem(
    int s,
    int flags,
    char *resource,
    size_t resourcelen,
    char *host,
    size_t hostlen,
    struct dill_ws_storage *mem,
    int64_t deadline);
DILL_EXPORT int dill_ws_send(
    int s,
    int flags,
    const void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT ssize_t dill_ws_recv(
    int s,
    int *flags,
    void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT int dill_ws_sendl(
    int s,
    int flags,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline);
DILL_EXPORT ssize_t dill_ws_recvl(
    int s,
    int *flags,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline);
DILL_EXPORT int dill_ws_done(
    int s,
    int status,
    const void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT int dill_ws_detach(
    int s,
    int status,
    const void *buf,
    size_t len,
    int64_t deadline);
DILL_EXPORT ssize_t dill_ws_status(
    int s,
    int *status,
    void *buf,
    size_t len);

/* Helper functions for those who want to implement HTTP exchange by hand. */

#define WS_KEY_SIZE 32

DILL_EXPORT int dill_ws_request_key(
    char *request_key);
DILL_EXPORT int dill_ws_response_key(
    const char *request_key,
    char *response_key);

#if !defined DILL_DISABLE_RAW_NAMES
#define WS_BINARY DILL_WS_BINARY
#define WS_TEXT DILL_WS_TEXT
#define WS_NOHTTP DILL_WS_NOHTTP
#define ws_storage dill_ws_storage
#define ws_attach_server dill_ws_attach_server
#define ws_attach_server_mem dill_ws_attach_server_mem
#define ws_attach_client dill_ws_attach_client
#define ws_attach_client_mem dill_ws_attach_client_mem
#define ws_send dill_ws_send
#define ws_recv dill_ws_recv
#define ws_sendl dill_ws_sendl
#define ws_recvl dill_ws_recvl
#define ws_done dill_ws_done
#define ws_detach dill_ws_detach
#define ws_status dill_ws_status
#define ws_request_key dill_ws_request_key
#define ws_response_key dill_ws_response_key
#endif

/******************************************************************************/
/*  SOCKS5                                                                    */
/******************************************************************************/

// SOCKS5 client commands
#define DILL_SOCKS5_CONNECT (0x01)
#define DILL_SOCKS5_BIND (0x02)
#define DILL_SOCKS5_UDP_ASSOCIATE (0x03)

// SOCKS5 server reply codes
#define DILL_SOCKS5_SUCCESS (0x00)
#define DILL_SOCKS5_GENERAL_FAILURE (0x01)
#define DILL_SOCKS5_CONNECTION_NOT_ALLOWED (0x02)
#define DILL_SOCKS5_NETWORK_UNREACHABLE (0x03)
#define DILL_SOCKS5_HOST_UNREACHABLE (0x04)
#define DILL_SOCKS5_CONNECTION_REFUSED (0x05)
#define DILL_SOCKS5_TTL_EXPIRED (0x06)
#define DILL_SOCKS5_COMMAND_NOT_SUPPORTED (0x07)
#define DILL_SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED (0x08)


typedef int dill_socks5_auth_function(const char *username,
    const char *password);

DILL_EXPORT int dill_socks5_client_connect(
    int s, const char *username, const char *password,
    struct dill_ipaddr *addr, int64_t deadline);

DILL_EXPORT int dill_socks5_client_connectbyname(
    int s, const char *username, const char *password, const char *hostname,
    int port, int64_t deadline);

DILL_EXPORT int dill_socks5_proxy_auth(
    int s, dill_socks5_auth_function *auth_fn, int64_t deadline);

DILL_EXPORT int dill_socks5_proxy_recvcommand(
    int s, struct dill_ipaddr *ipaddr, int64_t deadline);

DILL_EXPORT int dill_socks5_proxy_recvcommandbyname(
    int s, char *host, int *port, int64_t deadline);

DILL_EXPORT int dill_socks5_proxy_sendreply(
    int s, int reply, struct dill_ipaddr *ipaddr, int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES

#define socks5_client_connect dill_socks5_client_connect
#define socks5_client_connectbyname dill_socks5_client_connectbyname
#define socks5_proxy_auth dill_socks5_proxy_auth
#define socks5_proxy_recvcommand dill_socks5_proxy_recvcommand
#define socks5_proxy_recvcommandbyname dill_socks5_proxy_recvcommandbyname
#define socks5_proxy_sendreply dill_socks5_proxy_sendreply

#define SOCKS5_CONNECT DILL_SOCKS5_CONNECT
#define SOCKS5_BIND DILL_SOCKS5_BIND
#define SOCKS5_UDP_ASSOCIATE DILL_SOCKS5_UDP_ASSOCIATE

#define SOCKS5_SUCCESS DILL_SOCKS5_SUCCESS
#define SOCKS5_GENERAL_FAILURE DILL_SOCKS5_GENERAL_FAILURE
#define SOCKS5_CONNECTION_NOT_ALLOWED DILL_SOCKS5_CONNECTION_NOT_ALLOWED
#define SOCKS5_NETWORK_UNREACHABLE DILL_SOCKS5_NETWORK_UNREACHABLE
#define SOCKS5_HOST_UNREACHABLE DILL_SOCKS5_HOST_UNREACHABLE
#define SOCKS5_CONNECTION_REFUSED DILL_SOCKS5_CONNECTION_REFUSED
#define SOCKS5_TTL_EXPIRED DILL_SOCKS5_TTL_EXPIRED
#define SOCKS5_COMMAND_NOT_SUPPORTED DILL_SOCKS5_COMMAND_NOT_SUPPORTED
#define SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED DILL_SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED

#endif /* !defined DILL_DISABLE_RAW_NAMES */

/******************************************************************************/
/*  TERM protocol.                                                            */
/*  Implementes terminal handshake on the top of any message-based protocol.  */
/******************************************************************************/

struct dill_term_storage {char _[88];};

DILL_EXPORT int dill_term_attach(
    int s,
    const void *buf,
    size_t len);
DILL_EXPORT int dill_term_attach_mem(
    int s,
    const void *buf,
    size_t len,
    struct dill_term_storage *mem);
DILL_EXPORT int dill_term_done(
    int s,
    int64_t deadline);
DILL_EXPORT int dill_term_detach(
    int s,
    int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES
#define term_storage dill_term_storage
#define term_attach dill_term_attach
#define term_attach_mem dill_term_attach_mem
#define term_done dill_term_done
#define term_detach dill_term_detach
#endif

/******************************************************************************/
/* Happy Eyeballs (RFC 8305).                                                 */
/* Implements concurrent TCP connecting to the remote endpoint.               */
/******************************************************************************/

int dill_happyeyeballs_connect(const char *name, int port, int64_t deadline);

#if !defined DILL_DISABLE_RAW_NAMES
#define happyeyeballs_connect dill_happyeyeballs_connect
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif

