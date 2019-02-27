/*

  Copyright (c) 2019 Martin Sustrik

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
/*  Handles                                                                   */
/******************************************************************************/

DILL_EXPORT int dill_hclose(int h);
DILL_EXPORT int dill_hnullify(int h);

#if !defined DILL_DISABLE_RAW_NAMES
#define hclose dill_hclose
#define hnullify dill_hnullify
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

struct dill_bundle_storage {char _[64];};

struct dill_bundle_opts {
    struct dill_bundle_storage *mem;
};

DILL_EXPORT extern const struct dill_bundle_opts dill_bundle_defaults;

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

struct dill_chopts {
    struct dill_chstorage *mem;
};

DILL_EXPORT extern const struct dill_chopts dill_chdefaults;

#if !defined DILL_DISABLE_RAW_NAMES
#define CHSEND DILL_CHSEND
#define CHRECV DILL_CHRECV
#define chclause dill_chclause
#define chstorage dill_chstorage
#define chopts dill_chopts
#define chdefaults dill_chdefaults
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

struct dill_ipaddr {char _[32];};

#if !defined DILL_DISABLE_RAW_NAMES
#define IPADDR_IPV4 DILL_IPADDR_IPV4 
#define IPADDR_IPV6 DILL_IPADDR_IPV6
#define IPADDR_PREF_IPV4 DILL_IPADDR_PREF_IPV4 
#define IPADDR_PREF_IPV6 DILL_IPADDR_PREF_IPV6
#define IPADDR_MAXSTRLEN DILL_IPADDR_MAXSTRLEN
#define ipaddr_opts dill_ipaddr_opts
#define ipaddr_defaults dill_ipaddr_defaults
#define ipaddr dill_ipaddr
#endif

/******************************************************************************/
/*  TCP protocol.                                                             */
/******************************************************************************/

struct dill_tcp_storage {char _[72];};

struct dill_tcp_opts {
    struct dill_tcp_storage *mem;
    int backlog;
    unsigned int rx_buffering : 1; /* TODO: Make this the size of the buffer. */
    unsigned int nodelay : 1;
};

DILL_EXPORT extern const struct dill_tcp_opts dill_tcp_defaults;

#if !defined DILL_DISABLE_RAW_NAMES
#define tcp_opts dill_tcp_opts
#define tcp_defaults dill_tcp_defaults
#define tcp_storage dill_tcp_storage
#endif

/******************************************************************************/
/*  IPC protocol.                                                            */
/******************************************************************************/

struct dill_ipc_storage {char _[72];};

struct dill_ipc_opts {
    struct dill_ipc_storage *mem;
    int backlog;
    unsigned int rx_buffering : 1;
};

DILL_EXPORT extern const struct dill_ipc_opts dill_ipc_defaults;

#if !defined DILL_DISABLE_RAW_NAMES
#define ipc_opts dill_ipc_opts
#define ipc_defaults dill_ipc_defaults
#define ipc_storage dill_ipc_storage
#endif

/******************************************************************************/
/*  PREFIX protocol.                                                          */
/*  Messages are prefixed by size.                                            */
/******************************************************************************/

struct dill_prefix_storage {char _[56];};

struct dill_prefix_opts {
    struct dill_prefix_storage *mem;
    unsigned int little_endian : 1;
};

DILL_EXPORT extern const struct dill_prefix_opts dill_prefix_defaults;

#if !defined DILL_DISABLE_RAW_NAMES
#define prefix_opts dill_prefix_opts
#define prefix_defaults dill_prefix_defaults
#define prefix_storage dill_prefix_storage
#endif

/******************************************************************************/
/*  WebSockets protocol.                                                      */
/******************************************************************************/

struct dill_ws_storage {char _[176];};

struct dill_ws_opts {
    struct dill_ws_storage *mem;
    unsigned int http : 1;
    unsigned int text : 1;
};

DILL_EXPORT extern const struct dill_ws_opts dill_ws_defaults;

#define WS_KEY_SIZE 32

#if !defined DILL_DISABLE_RAW_NAMES
#define ws_opts dill_ws_opts
#define ws_defaults dill_ws_defaults
#define ws_storage dill_ws_storage
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

#endif

/******************************************************************************/
/* Generated stuff.                                                           */
/******************************************************************************/

@{hdrs}

#ifdef __cplusplus
}
#endif

#endif

