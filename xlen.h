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

#include "platform.h"

#define XLEN_MAX (255 + 16*255 + 16*32*255 + 16*32*127*65535U)

static inline uint32_t getxlen(const uchar **pp)
{
    uint32_t len = *(*pp)++;
    if (unlikely(len >= 256 - 16)) {
	uint32_t x = *(*pp)++;
	len += 16 * x;
	if (unlikely(x >= 256 - 32)) {
	    x = *(*pp)++;
	    len += 16 * 32 * x;
	    if (unlikely(x >= 256 - 127)) {
		x = load16le(*pp), *pp += 2;
		len += 16 * 32 * 127 * x;
	    }
	}
    }
    return len;
}

static inline void putxlen(uint32_t len, uchar **pp)
{
    uint n = 1;
    if (unlikely(len >= 256 - 16)) {
	uint32_t x = (len - (256 - 16)) / 16;
	len -= x * 16;
	n = 2;
	if (unlikely(x >= 256 - 32)) {
	    uint32_t y = (x - (256 - 32)) / 32;
	    x -= y * 32;
	    n = 3;
	    if (unlikely(y >= 256 - 127)) {
		uint32_t z = (y - (256 - 127)) / 127;
		y -= z * 127;
		n = 5;
		store16le(*pp + 3, z);
	    }
	    (*pp)[2] = y;
	}
	(*pp)[1] = x;
    }
    (*pp)[0] = len;
    (*pp) += n;
}
