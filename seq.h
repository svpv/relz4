#include "xlen.h"

#define MINOFF 16

#define Src (*psrc)
#define Out (*pout)

static inline void putseq(size_t llen, size_t mlen, uint32_t moff,
	const uchar **psrc, uchar **pout)
{
    uchar *tok = Out++;
    if (likely(llen <= 14)) {
	*tok = llen << 4;
	memcpy(Out, Src, 8);
	if (unlikely(llen > 8))
	    memcpy(Out + 8, Src + 8, 8);
	Src += llen, Out += llen;
    }
    else {
	*tok = 15 << 4;
	assert(llen < (3U<<30));
	putxlen(llen - 15, pout);
	const uchar *last8src = Src + llen - 8;
	      uchar *last8out = Out + llen - 8;
	do {
	    memcpy(Out, Src, 8);
	    Src += 8, Out += 8;
	} while (Src < last8src);
	memcpy(last8out, last8src, 8);
	Src = last8src + 8, Out = last8out + 8;
    }
    assert(moff >= MINOFF && moff <= MINOFF + UINT16_MAX);
    store16le(Out, moff - MINOFF);
    Out += 2;
    if (likely(mlen <= 19)) {
	if (unlikely(mlen == 19))
	    mlen = 18;
	else
	    assert(mlen >= 4);
	Src += mlen;
	*tok |= mlen - 4;
	return;
    }
    *tok |= 15;
    assert(mlen < (3U<<30));
    putxlen(mlen - 20, pout);
    Src += mlen;
}

static inline void putlastseq(size_t llen,
	const uchar **psrc, uchar **pout)
{
    uchar *tok = Out++;
    if (likely(llen <= 14)) {
	*tok = llen << 4;
	if (unlikely(llen > 8)) {
	    memcpy(Out + 0, Src + 0, 8);
	    Src += llen, Out += llen;
	    memcpy(Out - 8, Src - 8, 8);
	}
	else {
	    memcpy(Out + 0, Src + 0, 4);
	    Src += llen, Out += llen;
	    memcpy(Out - 4, Src - 4, 4);
	}
	return;
    }
    *tok = 15 << 4;
    putxlen(llen - 15, pout);
    const uchar *last8src = Src + llen - 8;
	  uchar *last8out = Out + llen - 8;
    do {
	memcpy(Out, Src, 8);
	Src += 8, Out += 8;
    } while (Src < last8src);
    memcpy(last8out, last8src, 8);
    Src = last8src + 8, Out = last8out + 8;
}
