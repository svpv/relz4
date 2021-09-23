#include "relz4.h"
#include "seq.h"

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
    const uchar *last5 = last12 + 7;
    assert(src <= last5);
    while (src < last5) {
	if (*src != *ref)
	    return len;
	src++, ref++, len++;
    }
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
    x *= 2246822519U;
    return x >> 17;
}

static inline void HC_update1(struct HC *hc, const uchar *src)
{
    uint32_t h = HC_hash(load32(src - MINOFF));
    uint32_t pos = hc->nextpos++;
    uint32_t d = pos - hc->htab[h];
    d = (d > UINT16_MAX) ? UINT16_MAX : d;
    hc->ctab[(uint16_t)pos] = d;
    hc->htab[h] = pos;
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

static inline uint32_t HC_find0(const struct HC *hc,
	const uchar *src, const uchar *last12,
	const uchar **pmstart, uint32_t *pmoff, int maxiter)
{
    uint32_t pos = src - hc->base;
    uint32_t pos0 = (pos > UINT16_MAX) ? pos - UINT16_MAX : 0;
    uint32_t src32 = load32(src);
    uint32_t mpos = hc->htab[HC_hash(src32)];
    uint32_t bestmlen = 3;
    *pmoff = NICEOFF;
    while (mpos >= pos0) {
	uint32_t d = hc->ctab[(uint16_t)mpos];
	const uchar *ref = hc->base + mpos;
	// probe for a longer match, unless the offset is small
	uint32_t probe = bestmlen + (*pmoff >= NICEOFF);
	if (load32(ref + probe - 4) != load32(src + probe - 4))
	    goto next;
	if (unlikely(load32(ref) != src32))
	    goto next;
	uint32_t mlen = 4 + HC_count(src + 4, ref + 4, last12);
	if (unlikely(mlen < bestmlen))
	    goto next;
	*pmoff = src - ref;
	bestmlen = mlen;
    next:
	if (--maxiter <= 0)
	    break;
	assert(mpos >= d);
	mpos -= d;
    }
    *pmstart = src;
    return bestmlen;
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
	if (load32(ref) != src32)
	    goto next;
	uint32_t mlen = 4 + HC_count(src + 4, ref + 4, last12);
	do {
	    if (ref == hc->base)
		break;
	    if (src[-1] != ref[-1])
		break;
	    src--, ref--, mlen++;
	} while (src > src0);
	if (mlen < bestmlen)
	    goto next;
	// a tie: lower start improves compression, offset not too small
	if (mlen == bestmlen && *pmstart <= src && *pmoff >= NICEOFF)
	    goto next;
	*pmstart = src;
	*pmoff = src - ref;
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
	mlen = HC_find0(&hc, src, last12, &mstart, &moff, 2 * maxiter);
	while (mlen < 4) {
	    if (++src > last12)
		goto outbreak;
	    HC_update1(&hc, src);
	    mlen = HC_find(&hc, src0, src, last12, &mstart, &moff, maxiter);
	}
    save1:
	mstart0 = mstart, moff0 = moff, mlen0 = mlen;
    search2:
	src = mstart + mlen - 2 - (mlen > 4);
	mlen2 = 0;
	if (likely(src <= last12)) {
	    HC_update(&hc, src);
	    mlen2 = HC_find(&hc, src0, src, last12, &mstart2, &moff2, maxiter);
	}
	if (likely(mlen2 <= mlen)) {
	    putseq(mstart - src0, mlen, moff, &src0, &out, &puttok);
	    src = src0;
	    continue;
	}
    found2:
	if (unlikely(mstart2 <= mstart)) {
	    mstart = mstart2, moff = moff2, mlen = mlen2;
	    if (likely(mstart <= mstart0))
		goto save1;
	    goto search2;
	}
	assert(mstart0 >= src0);
	if (unlikely(mstart0 < mstart) && likely(mstart2 < mstart + mlen0))
	    mstart = mstart0, moff = moff0, mlen = mlen0;
	if (likely(mstart2 - mstart < 3)) {
	    mstart = mstart2, moff = moff2, mlen = mlen2;
	    goto search2;
	}
	if (unlikely(mstart2 >= mstart + mlen)) {
	    putseq(mstart - src0, mlen, moff, &src0, &out, &puttok);
	    mstart = mstart2, moff = moff2, mlen = mlen2;
	    goto save1;
	}
    have12:
	// deal with the boundary between M and M2
	if (likely(mstart2 - mstart < OPTMLEN)) {
	    uint32_t nlen = (mlen > OPTMLEN) ? OPTMLEN : mlen;
	    intptr_t d = mstart + nlen - mstart2;
	    assert(d > 0);
	    mstart2 += d, mlen2 -= d;
	    assert(mlen2 >= 4);
	    // do not curtail M just yet, may have to deal with another M2
	}
    search3:
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
	// if M and M3 overlap or adjacent, M2 is useless, M3 becomes M2
	if (mstart3 <= mstart + mlen) {
	    moff2 = moff3, mlen2 = mlen3;
	    if (mstart3 < mstart2) {
		mstart2 = mstart3;
		goto found2;
	    }
	    mstart2 = mstart3;
	    goto search3;
	}
	// not enough space for M2 between M and M3? remove M2!
	if (mstart3 < mstart + mlen + 3) {
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
	mstart0 = mstart, moff0 = moff, mlen0 = mlen;
	mstart2 = mstart3, moff2 = moff3, mlen2 = mlen3;
	goto have12;
    }
outbreak:
    putlastseq(srcEnd - src0, &src0, &out, &puttok);
    return out;
}

size_t RELZ4_compressHC(const void *src, size_t srcSize, void *out, int level)
{
    return HC_compress(src, srcSize, out, 1 << (level - 1)) - (uchar *) out;
}
