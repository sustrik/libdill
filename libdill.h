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
/*  read and understand the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define DILL_VERSION_CURRENT 10

/*  The latest revision of the current interface. */
#define DILL_VERSION_REVISION 0

/*  How many past interface versions are still supported. */
#define DILL_VERSION_AGE 0

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

struct hvfs {
    void *(*query)(struct hvfs *vfs, const void *type);
    void (*close)(struct hvfs *vfs);
    /* Reserved. Do not use directly! */
    unsigned int refcount;
};

DILL_EXPORT int hmake(struct hvfs *vfs);
DILL_EXPORT void *hquery(int h, const void *type);
DILL_EXPORT int hdup(int h);
DILL_EXPORT int hclose(int h);

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

#define coroutine __attribute__((noinline))

DILL_EXPORT extern volatile void *dill_unoptimisable;

DILL_EXPORT __attribute__((noinline)) int dill_prologue(sigjmp_buf **ctx,
    void **ptr, size_t len, const char *file, int line);
DILL_EXPORT __attribute__((noinline)) void dill_epilogue(void);

/* In the following macros alloca(sizeof(size_t)) is used because clang
   doesn't support alloca with size zero. */

/* This assembly setjmp/longjmp mechanism is in the same order as glibc and
   musl but glibc implements pointer mangling, which is hard to support.
   This should be binary-compatible with musl though. */

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

/* Stack-switching on other microarchiterctures. */
#else
#define dill_setjmp(ctx) sigsetjmp(ctx, 0)
#define dill_longjmp(ctx) siglongjmp(ctx, 1)
/* For newer GCCs, -fstack-protector breaks on this; use -fno-stack-protector.
   Alternatively, implement custom DILL_SETSP for your microarchitecture. */
#define DILL_SETSP(x) \
    dill_unoptimisable = alloca((char*)alloca(sizeof(size_t)) - (char*)(x));
#endif

/* Statement expressions are a gcc-ism but they are also supported by clang.
   Given that there's no other way to do this, screw other compilers for now.
   See https://gcc.gnu.org/onlinedocs/gcc-3.2/gcc/Statement-Exprs.html */

#define go_mem(fn, ptr, len) \
    ({\
        sigjmp_buf *ctx;\
        void *stk = (ptr);\
        int h = dill_prologue(&ctx, &stk, (len), __FILE__, __LINE__);\
        if(h >= 0) {\
            if(!dill_setjmp(*ctx)) {\
                DILL_SETSP(stk);\
                fn;\
                dill_epilogue();\
            }\
        }\
        h;\
    })

#define go(fn) go_mem(fn, NULL, 0)

DILL_EXPORT int yield(void);
DILL_EXPORT int msleep(int64_t deadline);
DILL_EXPORT void fdclean(int fd);
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
    char reserved[36];
#else
    char reserved[72];
#endif
};

DILL_EXPORT int chmake(size_t itemsz);
DILL_EXPORT int chmake_mem(size_t itemsz, struct chmem *mem);
DILL_EXPORT int chsend(int ch, const void *val, size_t len, int64_t deadline);
DILL_EXPORT int chrecv(int ch, void *val, size_t len, int64_t deadline);
DILL_EXPORT int chdone(int ch);
DILL_EXPORT int choose(struct chclause *clauses, int nclauses,
    int64_t deadline);

#endif

