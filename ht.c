#include "relz4.h"
#include "seq.h"

#define OPTMLEN 18
#define NICEOFF 64

static inline uint32_t HT_count(const uchar *src,
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

struct HT {
    uint64_t mpos[1<<14];
    const uchar *base;
    uint32_t nextpos;
};

static inline uint32_t HT_hash(uint32_t x)
{
    x *= 2654435761U;
    return x >> 18;
}

static inline void HT_update1(struct HT *ht, const uchar *src)
{
    uint32_t h = HT_hash(load32(src - MINOFF));
    uint32_t pos = ht->nextpos++;
    ht->mpos[h] = ht->mpos[h] << 16 | (uint16_t) pos;
}

static inline void HT_update(struct HT *ht, const uchar *src)
{
    uint32_t pos = ht->nextpos;
    assert(src - ht->base > MINOFF-1);
    ht->nextpos = src - ht->base - (MINOFF-1);
    assert(pos < ht->nextpos);
    do {
	uint32_t h = HT_hash(load32(ht->base + pos));
	ht->mpos[h] = ht->mpos[h] << 16 | (uint16_t) pos;
    } while (++pos < ht->nextpos);
}

static inline uint32_t HT_find0(const struct HT *ht,
	const uchar *src, const uchar *last12,
	const uchar **pmstart, uint32_t *pmoff)
{
    uint32_t pos = src - ht->base;
    uint32_t src32 = load32(src);
    uint64_t mpos = ht->mpos[HT_hash(src32)];
    uint32_t bestmlen = 3;
    *pmoff = NICEOFF;
    for (int i = 0; i < 4; i++, mpos >>= 16) {
	uint32_t moff = (uint16_t)(pos - mpos - MINOFF) + MINOFF;
	const uchar *ref = src - moff;
	if (load32(ref) != src32)
	    continue;
	// probe for a longer match, unless the offset is small
	uint32_t probe = bestmlen + (*pmoff >= NICEOFF);
	if (load32(ref + probe - 4) != load32(src + probe - 4))
	    continue;
	uint32_t mlen = 4 + HT_count(src + 4, ref + 4, last12);
	if (mlen < bestmlen)
	    continue;
	*pmoff = moff;
	bestmlen = mlen;
    }
    *pmstart = src;
    return bestmlen;
}

static inline uint32_t HT_find(const struct HT *ht,
	const uchar *src0, const uchar *src1, const uchar *last12,
	const uchar **pmstart, uint32_t *pmoff)
{
    uint32_t pos = src1 - ht->base;
    uint32_t src32 = load32(src1);
    uint64_t mpos = ht->mpos[HT_hash(src32)];
    uint32_t bestmlen = 0;
    for (int i = 0; i < 4; i++, mpos >>= 16) {
	uint32_t moff = (uint16_t)(pos - mpos - MINOFF) + MINOFF;
	const uchar *src = src1;
	const uchar *ref = src - moff;
	if (load32(ref) != src32)
	    continue;
	uint32_t mlen = 4 + HT_count(src + 4, ref + 4, last12);
	while (src > src0 && ref > ht->base && src[-1] == ref[-1])
	    src--, ref--, mlen++;
	if (mlen < bestmlen)
	    continue;
	// a tie: lower start improves compression, offset not too small
	if (mlen == bestmlen && *pmstart <= src && *pmoff >= NICEOFF)
	    continue;
	*pmstart = src;
	*pmoff = moff;
	bestmlen = mlen;
    }
    return bestmlen;
}

static uchar *HT_compress(const uchar *src, size_t srcSize, uchar *out)
{
    struct HT ht;
    ht.nextpos = 0;
    ht.base = src;
    memset(ht.mpos, 0x00, sizeof ht.mpos);
    const uchar *src0 = src;
    const uchar *srcEnd = src + srcSize;
    const uchar *last12 = srcEnd - 12;
    const uchar *mstart, *mstart0, *mstart2;
    uchar *puttok = NULL;
    uint32_t moff, moff0, moff2;
    uint32_t mlen, mlen0, mlen2;
    src += MINOFF;
    while (src <= last12) {
	HT_update(&ht, src);
	mlen = HT_find0(&ht, src, last12, &mstart, &moff);
	while (mlen < 4) {
	    if (++src > last12)
		goto outbreak;
	    HT_update1(&ht, src);
	    mlen = HT_find(&ht, src0, src, last12, &mstart, &moff);
	}
    save1:
	mstart0 = mstart, moff0 = moff, mlen0 = mlen;
    search2:
	src = mstart + mlen - 2 - (mlen > 4);
	mlen2 = 0;
	if (src <= last12) {
	    HT_update(&ht, src);
	    mlen2 = HT_find(&ht, src0, src, last12, &mstart2, &moff2);
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
	// deal with the boundary between M and M2
	if (mstart2 - mstart < OPTMLEN) {
	    mlen = (mlen > OPTMLEN) ? OPTMLEN : mlen;
	    intptr_t d = mstart + mlen - mstart2;
	    if (d > 0) {
		mstart2 += d, mlen2 -= d;
		assert(mlen2 >= 4);
	    }
	}
	if (mstart + mlen > mstart2)
	    mlen = mstart2 - mstart;
	putseq(mstart - src0, mlen, moff, &src0, &out, &puttok);
	mstart = mstart2, moff = moff2, mlen = mlen2;
	goto save1;
    }
outbreak:
    putlastseq(srcEnd - src0, &src0, &out, &puttok);
    return out;
}

size_t RELZ4_compressHT(const void *src, size_t srcSize, void *out)
{
    return HT_compress(src, srcSize, out) - (uchar *) out;
}
