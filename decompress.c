#include "relz4.h"
#include "xlen.h"

static inline uchar *decompress(const uchar *src, size_t srcSize, uchar *out)
{
    const uchar *srcLast5 = src + srcSize - 5;
    while (1) {
	uint llen = *src++;
	uint mlen = llen;
	llen >>= 4;
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
	mlen &= 15;
	const uchar *ref = out - moff;
	if (likely(mlen != 15)) {
	    memcpy(out + 0, ref - 16, 16);
	    src += 2;
	    memcpy(out + 16, ref + 0, 2);
	    mlen += 4;
	    out += mlen;
	}
	else {
	    src += 2;
	    mlen = 20 + getxlen(&src);
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
