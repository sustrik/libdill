/*

  Copyright (c) 2017 Martin Sustrik
  Copyright (c) 2014 Wirebird Labs LLC.  All rights reserved.

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

#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

void dill_print_size(char x) {
}

int dill_maxfds(void) {
    /* Get the maximum number of file descriptors. */
    struct rlimit rlim;
    int rc = getrlimit(RLIMIT_NOFILE, &rlim);
    dill_assert(rc == 0);
    int maxfds = rlim.rlim_max;
#if defined BSD
    /* On newer versions of OSX, the above behaves weirdly and returns -1, 
       so use OPEN_MAX instead. */
    if(maxfds < 0) return OPEN_MAX;
#endif
    return maxfds;
}

uint16_t dill_gets(const uint8_t *buf) {
    return (((uint16_t)buf[0]) << 8) |
           ((uint16_t)buf[1]);
}

void dill_puts(uint8_t *buf, uint16_t val) {
    buf[0] = (uint8_t)(((val) >> 8) & 0xff);
    buf[1] = (uint8_t)(val & 0xff);
}

uint32_t dill_getl(const uint8_t *buf) {
    return (((uint32_t)buf[0]) << 24) |
           (((uint32_t)buf[1]) << 16) |
           (((uint32_t)buf[2]) << 8) |
           ((uint32_t)buf[3]);
}

void dill_putl(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(((val) >> 24) & 0xff);
    buf[1] = (uint8_t)(((val) >> 16) & 0xff);
    buf[2] = (uint8_t)(((val) >> 8) & 0xff);
    buf[3] = (uint8_t)(val & 0xff);
}

uint64_t dill_getll(const uint8_t *buf) {
    return (((uint64_t)buf[0]) << 56) |
           (((uint64_t)buf[1]) << 48) |
           (((uint64_t)buf[2]) << 40) |
           (((uint64_t)buf[3]) << 32) |
           (((uint64_t)buf[4]) << 24) |
           (((uint64_t)buf[5]) << 16) |
           (((uint64_t)buf[6]) << 8) |
           (((uint64_t)buf[7] << 0));
}

void dill_putll(uint8_t *buf, uint64_t val) {
    buf[0] = (uint8_t)((val >> 56) & 0xff);
    buf[1] = (uint8_t)((val >> 48) & 0xff);
    buf[2] = (uint8_t)((val >> 40) & 0xff);
    buf[3] = (uint8_t)((val >> 32) & 0xff);
    buf[4] = (uint8_t)((val >> 24) & 0xff);
    buf[5] = (uint8_t)((val >> 16) & 0xff);
    buf[6] = (uint8_t)((val >> 8) & 0xff);
    buf[7] = (uint8_t)(val & 0xff);
}

const char *dill_lstrip(const char *string, char delim) {
    const char *pos = string;
    while(*pos && *pos == delim) ++pos;
    return pos;
}

const char *dill_rstrip(const char *string, char delim) {
    const char *end = string + strlen(string) - 1;
    while(end > string && *end == delim) --end;
    return ++end;
}

int dill_random(uint8_t *buf, size_t len) {
    static int fd = -1;
    if(dill_slow(fd < 0)) {
        fd = open("/dev/urandom", O_RDONLY);
        dill_assert(fd >= 0);
    }
    ssize_t sz = read(fd, buf, len);
    if(dill_slow(sz < 0)) return -1;
    dill_assert(sz == len);
    return 0;
}

int dill_base64_decode(const char *in, size_t in_len, uint8_t *out,
      size_t out_len) {
    unsigned ii;
    unsigned io;
    unsigned rem;
    uint32_t v;
    uint8_t ch;

    /*  Unrolled lookup of ASCII code points.
        0xFF represents a non-base64 valid character. */
    const uint8_t DECODEMAP[256] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xFF, 0xFF, 0x3F,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xFF,
        0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
        0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    for(io = 0, ii = 0, v = 0, rem = 0; ii < in_len; ii++) {
        if(isspace(in[ii]))
            continue;
        if(in[ii] == '=')
            break;
        ch = DECODEMAP[(int)(in[ii])];
        /*  Discard invalid characters as per RFC 2045. */
        if(ch == 0xFF)
            break; 
        v = (v << 6) | ch;
        rem += 6;
        if (rem >= 8) {
            rem -= 8;
            if(io >= out_len)
                return -1;
            out[io++] = (v >> rem) & 255;
        }
    }
    if(rem >= 8) {
        rem -= 8;
        if(io >= out_len)
            return -1;
        out[io++] = (v >> rem) & 255;
    }
    return io;
}

int dill_base64_encode(const uint8_t *in, size_t in_len, char *out,
      size_t out_len) {
    unsigned ii;
    unsigned io;
    unsigned rem;
    uint32_t v;
    uint8_t ch;

    const uint8_t ENCODEMAP[64] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    for(io = 0, ii = 0, v = 0, rem = 0; ii < in_len; ii++) {
        ch = in[ii];
        v = (v << 8) | ch;
        rem += 8;
        while(rem >= 6) {
            rem -= 6;
            if(io >= out_len)
                return -1;
            out[io++] = ENCODEMAP[(v >> rem) & 63];
        }
    }
    if(rem) {
        v <<= (6 - rem);
        if(io >= out_len)
            return -1;
        out[io++] = ENCODEMAP[v & 63];
    }
    /*  Pad to a multiple of 3. */
    while(io & 3) {
        if(io >= out_len)
            return -1;
        out[io++] = '=';
    }
    if(io >= out_len)
        return -1;
    out[io] = '\0';
    return io;
}

#define dill_sha1_rol32(num,bits) ((num << bits) | (num >> (32 - bits)))

void dill_sha1_init(struct dill_sha1 *self) {
    /*  Detect endianness. */
    union {
        uint32_t i;
        char c[4];
    } test = {0x00000001};
    self->is_little_endian = test.c[0];
    /*  Initial state of the hash. */
    self->state [0] = 0x67452301;
    self->state [1] = 0xefcdab89;
    self->state [2] = 0x98badcfe;
    self->state [3] = 0x10325476;
    self->state [4] = 0xc3d2e1f0;
    self->bytes_hashed = 0;
    self->buffer_offset = 0;
}

static void dill_sha1_add(struct dill_sha1 *self, uint8_t data) {
    uint8_t i;
    uint32_t a, b, c, d, e, t;
    uint8_t *const buf = (uint8_t*)self->buffer;
    if(self->is_little_endian)
        buf[self->buffer_offset ^ 3] = data;
    else
        buf[self->buffer_offset] = data;
    self->buffer_offset++;
    if(self->buffer_offset == DILL_SHA1_BLOCK_LEN) {
        a = self->state[0];
        b = self->state[1];
        c = self->state[2];
        d = self->state[3];
        e = self->state[4];
        for(i = 0; i < 80; i++) {
            if(i >= 16) {
                t = self->buffer[(i + 13) & 15] ^
                    self->buffer[(i + 8) & 15] ^
                    self->buffer[(i + 2) & 15] ^
                    self->buffer[i & 15];
                self->buffer[i & 15] = dill_sha1_rol32(t, 1);
            }
            if (i < 20)
                t = (d ^ (b & (c ^ d))) + 0x5A827999;
            else if(i < 40)
                t = (b ^ c ^ d) + 0x6ED9EBA1;
            else if(i < 60)
                t = ((b & c) | (d & (b | c))) + 0x8F1BBCDC;
            else
                t = (b ^ c ^ d) + 0xCA62C1D6;
            t += dill_sha1_rol32(a, 5) + e + self->buffer[i & 15];
            e = d;
            d = c;
            c = dill_sha1_rol32(b, 30);
            b = a;
            a = t;
        }
        self->state[0] += a;
        self->state[1] += b;
        self->state[2] += c;
        self->state[3] += d;
        self->state[4] += e;
        self->buffer_offset = 0;
    }
}

void dill_sha1_hashbyte(struct dill_sha1 *self, uint8_t data) {
    ++self->bytes_hashed;
    dill_sha1_add(self, data);
}

uint8_t *dill_sha1_result(struct dill_sha1 *self) {
    int i;
    /*  Pad to complete the last block. */
    dill_sha1_add(self, 0x80);
    while(self->buffer_offset != 56)
        dill_sha1_add (self, 0x00);
    /*  Append length in the last 8 bytes. SHA-1 supports 64-bit hashes, so
        zero-pad the top bits. Shifting to multiply by 8 as SHA-1 supports
        bit- as well as byte-streams. */
    dill_sha1_add(self, 0);
    dill_sha1_add(self, 0);
    dill_sha1_add(self, 0);
    dill_sha1_add(self, self->bytes_hashed >> 29);
    dill_sha1_add(self, self->bytes_hashed >> 21);
    dill_sha1_add(self, self->bytes_hashed >> 13);
    dill_sha1_add(self, self->bytes_hashed >> 5);
    dill_sha1_add(self, self->bytes_hashed << 3);
    /*  Correct byte order for little-endian systems. */
    if(self->is_little_endian) {
        for(i = 0; i < 5; i++) {
            self->state[i] =
                (((self->state[i]) << 24) & 0xFF000000) |
                (((self->state[i]) << 8) & 0x00FF0000) |
                (((self->state[i]) >> 8) & 0x0000FF00) |
                (((self->state[i]) >> 24) & 0x000000FF);
        }
    }
    /* 20-octet pointer to hash. */
    return (uint8_t*)self->state;
}

