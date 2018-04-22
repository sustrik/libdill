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

#ifndef DILL_UTILS_H_INCLUDED
#define DILL_UTILS_H_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define dill_concat(x,y) x##y

/* Defines a unique identifier of type const void*. */
#define dill_unique_id(name) \
    static const int dill_concat(name, ___) = 0;\
    const void *name = & dill_concat(name, ___);

/*  Takes a pointer to a member variable and computes pointer to the structure
    that contains it. 'type' is type of the structure, not the member. */
#define dill_cont(ptr, type, member) \
    (ptr ? ((type*) (((char*) ptr) - offsetof(type, member))) : NULL)

/* Compile-time assert. */
#define DILL_CT_ASSERT_HELPER2(prefix, line) \
    prefix##line
#define DILL_CT_ASSERT_HELPER1(prefix, line) \
    DILL_CT_ASSERT_HELPER2(prefix, line)
#define DILL_CT_ASSERT(x) \
    typedef int DILL_CT_ASSERT_HELPER1(ct_assert_,__COUNTER__) [(x) ? 1 : -1];

/* DILL_CHECK_STORAGE checks whether struct 'type' fits into struct 'storage'.
   If DILL_PRINT_SIZES macro is defined it will also print the size
   of 'type'. */
#if defined DILL_PRINT_SIZES
#define DILL_CHECK_STORAGE(type, storage) \
    static void dill_print_size_2_##type(void) { \
        char x[sizeof(struct type)]; \
        dill_print_size(&x); \
    } \
    DILL_CT_ASSERT(sizeof(struct type) <= sizeof(struct storage))
#else
#define DILL_CHECK_STORAGE(type, storage) \
    DILL_CT_ASSERT(sizeof(struct type) <= sizeof(struct storage))
#endif

void dill_print_size(char x);

/* Optimisation hints. */
#if defined __GNUC__ || defined __llvm__
#define dill_fast(x) __builtin_expect(!!(x), 1)
#define dill_slow(x) __builtin_expect(!!(x), 0)
#else
#define dill_fast(x) (x)
#define dill_slow(x) (x)
#endif

/* Define our own assert. This way we are sure that it stays in place even
   if the standard C assert would be thrown away by the compiler. It also
   allows us to overload it as needed. */
#define dill_assert(x) \
    do {\
        if (dill_slow(!(x))) {\
            fprintf(stderr, "Assert failed: " #x " (%s:%d)\n",\
                __FILE__, __LINE__);\
            fflush(stderr);\
            abort();\
        }\
    } while (0)

/* Workaround missing __rdtsc in Clang < 3.5 (or Clang < 6.0 on Xcode) */
#if defined(__x86_64__) || defined(__i386__)
#if defined __clang__
#if (!defined(__apple_build_version__) && \
    ((__clang_major__ < 3) || \
    ((__clang_major__ == 3) && (__clang_minor__ < 5)))) || \
    (defined(__apple_build_version__) && (__clang_major__ < 6))
static inline uint64_t __rdtsc() {
#if defined __i386__
    uint64_t x;
    asm volatile ("rdtsc" : "=A" (x));
    return x;
#else
    uint64_t a, d;
    asm volatile ("rdtsc" : "=a" (a), "=d" (d));
    return (d << 32) | a;
#endif
}
#endif
#endif
#endif

/* Returns the maximum possible file descriptor number */
int dill_maxfds(void);

/* Encoding and decoding integers from network byte order. */
uint16_t dill_gets(const uint8_t *buf);
void dill_puts(uint8_t *buf, uint16_t val);
uint32_t dill_getl(const uint8_t *buf);
void dill_putl(uint8_t *buf, uint32_t val);
uint64_t dill_getll(const uint8_t *buf);
void dill_putll(uint8_t *buf, uint64_t val);

/* Returns a pointer to the first character in string that is not delim. */
const char *dill_lstrip(const char *string, char delim);
/* Returns a pointer after the last character in string that is not delim. */
const char *dill_rstrip(const char *string, char delim);

/* Cryptographically random bytes. */
int dill_random(uint8_t *buf, size_t len);

/******************************************************************************/
/* Base64 functions are based on base64.c (Public Domain) by Jon Mayo.        */
/******************************************************************************/

int dill_base64_encode(const uint8_t *in, size_t in_len,
    char *out, size_t out_len);
int dill_base64_decode(const char *in, size_t in_len,
    uint8_t *out, size_t out_len);

/******************************************************************************/
/*  SHA-1 SECURITY NOTICE:                                                    */
/*  The algorithm as designed below is not intended for general purpose use.  */
/*  As-designed, it is a single-purpose function for this WebSocket           */
/*  Opening Handshake. As per RFC 6455 10.8, SHA-1 usage "doesn't depend on   */
/*  any security properties of SHA-1, such as collision resistance or         */
/*  resistance to the second pre-image attack (as described in [RFC4270])".   */
/*  Caveat emptor for uses of this function elsewhere.                        */
/*                                                                            */
/*  Based on sha1.c (Public Domain) by Steve Reid, these functions calculate  */
/*  the SHA1 hash of arbitrary byte locations byte-by-byte.                   */
/******************************************************************************/

#define DILL_SHA1_HASH_LEN 20
#define DILL_SHA1_BLOCK_LEN 64

struct dill_sha1 {
    uint32_t buffer[DILL_SHA1_BLOCK_LEN / sizeof (uint32_t)];
    uint32_t state[DILL_SHA1_HASH_LEN / sizeof (uint32_t)];
    uint32_t bytes_hashed;
    uint8_t buffer_offset;
    uint8_t is_little_endian;
};

void dill_sha1_init(struct dill_sha1 *self);
void dill_sha1_hashbyte(struct dill_sha1 *self, uint8_t data);
uint8_t *dill_sha1_result(struct dill_sha1 *self);

#endif

