// Copyright (c) 2020 Alexey Tourbin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(x, 0)

#define inline inline __attribute__((always_inline))

static inline uint16_t load16le(const void *p)
{
    uint16_t x;
    memcpy(&x, p, 2);
#if defined(__GNUC__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    x = __builtin_bswap16(x);
#endif
    return x;
}

static inline void store16le(void *p, uint16_t x)
{
#if defined(__GNUC__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    x = __builtin_bswap16(x);
#endif
    memcpy(p, &x, 2);
}

static inline uint32_t load32(const void *p)
{
    uint32_t x;
    memcpy(&x, p, 4);
    return x;
}

static inline uint64_t load64(const void *p)
{
    uint64_t x;
    memcpy(&x, p, 8);
    return x;
}

// ~/.vim/after/syntax/c.vim:
// syn keyword cType uchar uint
typedef unsigned char uchar;
typedef unsigned int uint;
