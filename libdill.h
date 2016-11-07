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
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

/******************************************************************************/
/*  ABI versioning support                                                    */
/******************************************************************************/

/*  Don't change this unless you know exactly what you're doing and have      */
/*  read and understand the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define DILL_VERSION_CURRENT 6

/*  The latest revision of the current interface. */
#define DILL_VERSION_REVISION 0

/*  How many past interface versions are still supported. */
#define DILL_VERSION_AGE 0

/******************************************************************************/
/*  Symbol visibility                                                         */
/******************************************************************************/

/* Define the amount of memory to allocate through alloca */
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
};

DILL_EXPORT int hcreate(struct hvfs *vfs);
DILL_EXPORT void *hquery(int h, const void *type);
DILL_EXPORT int hdup(int h);
DILL_EXPORT int hclose(int h);

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

/* Statement expressions are a gcc-ism but they are also supported by clang.
   Given that there's no other way to do this, screw other compilers for now.
   See https://gcc.gnu.org/onlinedocs/gcc-3.2/gcc/Statement-Exprs.html */

#define DILL_NOINLINE __attribute__((noinline))
#define coroutine DILL_NOINLINE

DILL_EXPORT extern volatile int dill_unoptimisable1;
DILL_EXPORT extern volatile void *dill_unoptimisable2;

DILL_EXPORT DILL_NOINLINE int dill_prologue(void **ctx, void **stk, size_t len,
    const char *file, int line);
DILL_EXPORT DILL_NOINLINE void dill_epilogue(void);
DILL_EXPORT int dill_proc_prologue(int *hndl);
DILL_EXPORT void dill_proc_epilogue(void);

#include <signal.h>
#if defined(__x86_64__)
#define dill_setjmp(ctx) ({\
    int ret;\
    asm("lea     LJMPRET%=(%%rip), %%rcx\n\t"\
        "xor     %%rax, %%rax\n\t"\
        "mov     %%rcx, (%%rdx)\n\t"\
        "mov     %%rsp, 8(%%rdx)\n\t"\
        "mov     %%r12, 16(%%rdx)\n\t"\
        "mov     %%rbp, 24(%%rdx)\n\t"\
        "mov     %%r13, 32(%%rdx)\n\t"\
        "mov     %%r14, 40(%%rdx)\n\t"\
        "mov     %%r15, 48(%%rdx)\n\t"\
        "mov     %%rbx, 56(%%rdx)\n\t"\
        "mov     %%rdi, 64(%%rdx)\n\t"\
        "mov     %%rsi, 72(%%rdx)\n\t"\
        "LJMPRET%=:\n\t"\
        : "=a" (ret)\
        : "d" (ctx)\
        : "memory", "rcx", "r8", "r9", "r10", "r11", "cc");\
    ret;\
})
#define dill_longjmp(ctx) ({\
    asm("movq   (%%rax), %%rdx\n\t"\
        "movq   8(%%rax), %%rsp\n\t"\
        "movq   16(%%rax), %%r12\n\t"\
        "movq   24(%%rax), %%rbp\n\t"\
        "movq   32(%%rax), %%r13\n\t"\
        "movq   40(%%rax), %%r14\n\t"\
        "movq   48(%%rax), %%r15\n\t"\
        "movq   56(%%rax), %%rbx\n\t"\
        "movq   64(%%rax), %%rdi\n\t"\
        "movq   72(%%rax), %%rsi\n\t"\
        "jmp    *%%rdx\n\t"\
        : : "a" (ctx) : "rdx");\
})
#define dill_sizeof_jmpbuf 80
#define dill_setsp(x) asm volatile("movq %0, %%rsp"::"r"(x))
#define dill_getsp() ({uintptr_t x;asm volatile("":"=rsp"(x));x;})
#define dill_dummyuse(a) asm(""::"r"(a))
#elif defined(__i386__)
#define dill_setjmp(ctx) ({\
    int ret;\
    asm("movl   $LJMPRET%=, %%eax\n\t"\
        "movl   %%eax, (%%edx)\n\t"\ "movl   %%ebx, 4(%%edx)\n\t"\
        "movl   %%esi, 8(%%edx)\n\t"\
        "movl   %%edi, 12(%%edx)\n\t"\
        "movl   %%ebp, 16(%%edx)\n\t"\
        "movl   %%esp, 20(%%edx)\n\t"\
        "xorl   %%eax, %%eax\n\t"\
        "LJMPRET%=:\n\t"\
        : "=a" (ret) : "d" (ctx) : "memory", "cc");\
    ret;\
})
#define dill_longjmp(ctx) \
    asm("movl   (%%eax), %%edx\n\t"\
        "movl   4(%%eax), %%ebx\n\t"\
        "movl   8(%%eax), %%esi\n\t"\
        "movl   12(%%eax), %%edi\n\t"\
        "movl   16(%%eax), %%ebp\n\t"\
        "movl   20(%%eax), %%esp\n\t"\
        "jmp    *%%edx\n\t"\
        : : "a" (ctx) : "edx" \
    )
#define dill_sizeof_jmpbuf 24
#define dill_setsp(x) asm volatile("movl %0, %%esp"::"r"(x));
#define dill_getsp() ({uintptr_t x;asm volatile("":"=esp"(x));x;})
#define dill_dummyuse(a) asm(""::"r"(a))
#else
#define DILL_NOASMSETSP
#endif

#if !defined dill_setjmp && !defined dill_longjmp && !dill_sizeof_jmpbuf
#include <setjmp.h>
#define dill_setjmp(ctx) sigsetjmp(*(sigjmp_buf *)ctx, 0)
#define dill_longjmp(ctx) siglongjmp(*(sigjmp_buf *)ctx, 1)
#define dill_sizeof_jmpbuf sizeof(sigjmp_buf)
#endif

#ifndef dill_dummyuse
#define DILL_UNOPTIMISABLE2
#define dill_dummyuse(a) (dill_unoptimisable2 = a)
#endif

/* Here be dragons: without assembly, the VLAs are necessary to coerce the
   compiler to always generate a stack frame
   (they are unimplementable without a stack frame).
   The stack frame lets fn reference the local variables, which store the
   function arguments needed, even when the stack pointer is changed.
   This code is wrapped in dill_gosp which, with VLAs, is fragile because it is not
   wrapped in itself.  These macros should only be used internally. */

/* In newer GCCs, -fstack-protector* breaks on VLAs and alloca, use -fno-stack-protector */
#ifdef DILL_NOASMSETSP
#if __STDC_VERSION__ == 199901L || (__STDC_VERSION__ == 201112L && !defined(__STDC_NO_VLA))
/* Implement dill_gosp using VLAs: this macro is FRAGILE */
#define dill_gosp(stk) \
   char dill_anchor[dill_unoptimisable1];\
   dill_dummyuse(&dill_anchor);\
   char dill_filler[(char *)&dill_anchor - (char *)stk];\
   dill_dummyuse(&dill_filler);
#elif HAVE_ALLOCA_H
#include <alloca.h>
#elif defined __GNUC__
#define alloca __builtin_alloca
#else
#include <stddef.h>
void *alloca (size_t);
# endif
/* Implement dill_gosp using alloca, is slightly slower than VLAs but more robust */
#if !defined dill_gosp && defined alloca
#if defined __GNUC__
#define dill_getsp() alloca(0)
#elif defined __clang__
#define dill_getsp() alloca(sizeof(size_t))
#endif
#define dill_setsp(stk) alloca((char *)dill_getsp() - (char *)stk)
#define dill_gosp(stk) dill_dummyuse(dill_setsp(stk))
#endif
#else
/* This forces a stack frame between two function calls. */
#define dill_forcestackframe() dill_dummyuse(__builtin_frame_address(0))
/* This works with newer GCCs and is a bit more optimised.
   However, dill_setsp needs to be implemented per architecture. */
#define dill_gosp(stk) \
    dill_forcestackframe(); \
    dill_setsp(stk);
#endif

#define DILL_ALLOC_CAPS_ZERO     0x0002
#define DILL_ALLOC_CAPS_BOOKKEEP 0x0004
#define DILL_ALLOC_CAPS_ALIGN    0x0020
#define DILL_ALLOC_CAPS_GUARD    0x0040

extern const void *alloc_type;
struct alloc_vfs {
    void *(*alloc)(struct alloc_vfs *);
    int (*free)(struct alloc_vfs *, void *);
    ssize_t (*size)(struct alloc_vfs *);
    int (*caps)(struct alloc_vfs *);
};

/* Memory Allocation methods are selected in two ways:
   - If a hint, dstkbest or dstkcompat, are chosen, then choose the best out of
     those selected here.
   - If no hints is chosen, then these flags are mutually exclusive. stk() will
     error with -1 and errno=ENOTSUP */
#define DILL_ALLOC_FLAGS_DEFAULT  0x0000
#define DILL_ALLOC_FLAGS_ZERO     0x0002
#define DILL_ALLOC_FLAGS_ALIGN    0x0020
#define DILL_ALLOC_FLAGS_GUARD    0x0040
#define DILL_ALLOC_FLAGS_HUGEPAGE 0x0080

/* Standard libdill memory allocators (returns a handle) */
extern const void *abasic_type;
DILL_EXPORT int abasic(int flags, size_t sz);

extern const void *apool_type;
DILL_EXPORT int apool(int flags, size_t sz, size_t poolsz);

extern const void *acache_type;
DILL_EXPORT int acache(int flags, size_t sz, size_t cachesz);

DILL_EXPORT void *aalloc(int h);
DILL_EXPORT int afree(int h, void *ptr);
DILL_EXPORT ssize_t asize(int h);
DILL_EXPORT int acaps(int h);

#define go_stack(fn, ptr, len) \
    ({\
        void *ctx, *stk = (ptr);\
        int h = dill_prologue(&ctx, &stk, (len), __FILE__, __LINE__);\
        if(h >= 0) {\
            if(!dill_setjmp(ctx)) {\
                dill_gosp(stk);\
                fn;\
                dill_epilogue();\
            }\
        }\
        h;\
    })

#define go(fn) go_stack(fn, NULL, 0)

#define proc(fn) \
    ({\
        int h;\
        if(dill_proc_prologue(&h)) {\
            fn;\
            dill_proc_epilogue();\
        }\
        h;\
    })

DILL_EXPORT int yield(void);
DILL_EXPORT int msleep(int64_t deadline);
DILL_EXPORT void fdclean(int fd);
DILL_EXPORT int fdin(int fd, int64_t deadline);
DILL_EXPORT int fdout(int fd, int64_t deadline);
DILL_EXPORT void *cls(void);
DILL_EXPORT void setcls(void *val);

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
    char reserved[64];
};

DILL_EXPORT int channel(size_t itemsz, size_t bufsz);
DILL_EXPORT int chsend(int ch, const void *val, size_t len, int64_t deadline);
DILL_EXPORT int chrecv(int ch, void *val, size_t len, int64_t deadline);
DILL_EXPORT int chdone(int ch);
DILL_EXPORT int choose(struct chclause *clauses, int nclauses,
    int64_t deadline);

#endif

