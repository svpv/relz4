#include "relz4.h"
#include "xlen.h"

static inline uchar *decompress(const uchar *src, size_t srcSize, uchar *out)
{
    const uchar *srcLast5 = src + srcSize - 5;
    uint tok = *src;
    uint llen = tok >> 4;
    src += 2;
    while (1) {
	uint mlen = tok;
	tok = src[-1];
	if (likely(mlen < 0xf0)) {
	    memcpy(out, src, 16);
	    src += llen, out += llen;
	}
	else {
	    llen += getxlen(&src);
	    uchar *outEnd = out + llen;
	    do {
		memcpy(out, src, 32);
		src += 32, out += 32;
	    } while (out < outEnd);
	    src -= (out - outEnd);
	    out = outEnd;
	}
	if (unlikely(src > srcLast5))
	    break;
	uint moff = load16le(src);
	llen = tok >> 4;
	const uchar *ref = out - moff;
	memcpy(out + 0, ref - 16, 16);
	mlen &= 15;
	if (likely(mlen != 0)) {
	    src += 3;
	    memcpy(out + 16, ref + 0, 2);
	    mlen += 3;
	    out += mlen;
	}
	else {
	    out += 16, ref += 16;
	    src += 2;
	    mlen = 20 - 16 + getxlen(&src);
	    src += 1;
	    uchar *outEnd = out + mlen;
	    do {
		memcpy(out + 0, ref - 16, 16);
		memcpy(out + 16, ref + 0, 16);
		ref += 32, out += 32;
	    } while (out < outEnd);
	    out = outEnd;
	}
    }
    assert(src == srcLast5 + 5);
    return out;
}

size_t RELZ4_decompress(const void *src, size_t srcSize, void *out)
{
    return decompress(src, srcSize, out) - (const uchar *) out;
}
