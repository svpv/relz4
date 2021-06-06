#include "relz4.h"
#include "seq.h"

#define OPTMLEN 18
#define NICEOFF 64

static inline uint32_t HC_count(const uchar *src,
	const uchar *ref, const uchar *last12)
{
    uint32_t len = 0;
    while (src < last12) {
	uint64_t x = load64(src);
	uint64_t y = load64(ref);
	uint64_t d = x ^ y;
	if (likely(d))
	    return len + (__builtin_ctzll(d) >> 3);
	src += 8, ref += 8, len += 8;
    }
    // TOOD
    return len;
}

struct HC {
    const uchar *base;
    uint32_t nextpos;
    uint16_t ctab[1<<16];
    uint32_t htab[1<<15];
};

static inline uint32_t HC_hash(uint32_t x)
{
    x *= 2654435761U;
    return x >> 17;
}

static inline void HC_update(struct HC *hc, const uchar *src)
{
    uint32_t pos = hc->nextpos;
    assert(src - hc->base > MINOFF-1);
    hc->nextpos = src - hc->base - (MINOFF-1);
    assert(pos < hc->nextpos);
    do {
	uint32_t h = HC_hash(load32(hc->base + pos));
	uint32_t d = pos - hc->htab[h];
	d = (d > UINT16_MAX) ? UINT16_MAX : d;
	hc->ctab[(uint16_t)pos] = d;
	hc->htab[h] = pos;
    } while (++pos < hc->nextpos);
}

static inline uint32_t HC_find(const struct HC *hc,
	const uchar *src0, const uchar *src1, const uchar *last12,
	const uchar **pmstart, uint32_t *pmoff, int maxiter)
{
    uint32_t pos = src1 - hc->base;
    uint32_t pos0 = (pos > UINT16_MAX) ? pos - UINT16_MAX : 0;
    uint32_t src32 = load32(src1);
    uint32_t mpos = hc->htab[HC_hash(src32)];
    uint32_t bestmlen = 0;
    while (mpos >= pos0) {
	uint32_t d = hc->ctab[(uint16_t)mpos];
	const uchar *src = src1;
	const uchar *ref = hc->base + mpos;
	uint32_t moff = src - ref;
	if (load32(ref) != src32)
	    goto next;
	uint32_t mlen = 4 + HC_count(src + 4, ref + 4, last12);
	while (src > src0 && ref > hc->base && src[-1] == ref[-1])
	    src--, ref--, mlen++;
	if (mlen < bestmlen)
	    goto next;
	// a tie: lower start improves compression, offset not too small
	if (mlen == bestmlen && *pmstart <= src && *pmoff >= NICEOFF)
	    goto next;
	*pmstart = src;
	*pmoff = moff;
	bestmlen = mlen;
    next:
	if (--maxiter <= 0)
	    break;
	assert(mpos >= d);
	mpos -= d;
    }
    return bestmlen;
}

static uchar *HC_compress(const uchar *src, size_t srcSize,
	uchar *out, int maxiter)
{
    struct HC hc;
    hc.nextpos = 0;
    hc.base = src;
    memset(hc.htab, 0x00, sizeof hc.htab);
    memset(hc.ctab, 0xff, sizeof hc.ctab);
    const uchar *src0 = src;
    const uchar *srcEnd = src + srcSize;
    const uchar *last12 = srcEnd - 12;
    const uchar *mstart, *mstart0, *mstart2, *mstart3;
    uchar *puttok = NULL;
    uint32_t moff, moff0, moff2, moff3;
    uint32_t mlen, mlen0, mlen2, mlen3;
    src += MINOFF;
    while (src <= last12) {
	HC_update(&hc, src);
	mlen = HC_find(&hc, src0, src, last12, &mstart, &moff, maxiter);
	if (mlen == 0) {
	    src++;
	    continue;
	}
    save1:
	mstart0 = mstart, moff0 = moff, mlen0 = mlen;
    search2:
	src = mstart + mlen - 2 - (mlen > 4);
	mlen2 = 0;
	if (src <= last12) {
	    HC_update(&hc, src);
	    mlen2 = HC_find(&hc, src0, src, last12, &mstart2, &moff2, maxiter);
	}
	if (mlen2 <= mlen) {
	    putseq(mstart - src0, mlen, moff, &src0, &out, &puttok);
	    src = src0;
	    continue;
	}
	if (mstart2 <= mstart) {
	    mstart = mstart2, moff = moff2, mlen = mlen2;
	    if (mstart <= mstart0)
		goto save1;
	    goto search2;
	}
	if (mstart0 < mstart && mstart2 < mstart + mlen0)
	    mstart = mstart0, moff = moff0, mlen = mlen0;
	if (mstart2 - mstart < 3) {
	    mstart = mstart2, moff = moff2, mlen = mlen2;
	    goto search2;
	}
	if (mstart2 >= mstart + mlen) {
	    putseq(mstart - src0, mlen, moff, &src0, &out, &puttok);
	    mstart = mstart2, moff = moff2, mlen = mlen2;
	    goto save1;
	}
    found2:
	// deal with the boundary between M and M2
	if (mstart2 - mstart < OPTMLEN) {
	    uint32_t nlen = (mlen > OPTMLEN) ? OPTMLEN : mlen;
	    intptr_t d = mstart + nlen - mstart2;
	    if (d > 0) {
		mstart2 += d, mlen2 -= d;
		assert(mlen2 >= 4);
	    }
	    // do not curtail M just yet, may have to deal with another M2
	}
	src = mstart2 + mlen2 - 3;
	mlen3 = 0;
	if (src <= last12) {
	    HC_update(&hc, src);
	    mlen3 = HC_find(&hc, src0, src, last12, &mstart3, &moff3, maxiter);
	}
	if (mlen3 <= mlen2) {
	    if (mstart + mlen > mstart2)
		mlen = mstart2 - mstart;
	    putseq(mstart - src0, mlen, moff, &src0, &out, &puttok);
	    putseq(mstart2 - src0, mlen2, moff2, &src0, &out, &puttok);
	    src = src0;
	    continue;
	}
	// not enouch space for M2 between M and M3? remove M2!
	if (mstart3 < mstart + mlen + 3) {
	    // if M and M3 overlap, M2 is useless, M3 becomes M2
	    if (mstart3 < mstart + mlen) {
		mstart2 = mstart3, moff2 = moff3, mlen2 = mlen3;
		goto found2;
	    }
	    // write M, M2 becomes M0, M3 becomes M
	    putseq(mstart - src0, mlen, moff, &src0, &out, &puttok);
	    intptr_t d = mstart + mlen - mstart2;
	    if (d > 0) {
		mstart2 += d, mlen2 -= d;
		assert(mlen2 >= 4);
	    }
	    mstart0 = mstart2, moff0 = moff2, mlen0 = mlen2;
	    mstart = mstart3, moff = moff3, mlen = mlen3;
	    goto search2;
	}
	// write M, M2 becomes M, M3 becomes M2
	if (mstart + mlen > mstart2)
	    mlen = mstart2 - mstart;
	putseq(mstart - src0, mlen, moff, &src0, &out, &puttok);
	mstart = mstart2, moff = moff2, mlen = mlen2;
	mstart2 = mstart3, moff2 = moff3, mlen2 = mlen3;
	goto found2;
    }
    putlastseq(srcEnd - src0, &src0, &out, &puttok);
    return out;
}

size_t RELZ4_compress(const void *src, size_t srcSize, void *out, int level)
{
    return HC_compress(src, srcSize, out, 1 << (level - 1)) - (uchar *) out;
}
