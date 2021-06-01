#pragma once
#ifndef __cplusplus
#include <stddef.h>
#else
#include <cstddef>
extern "C" {
#endif

#define RELZ4_COMPRESSBOUND(x) (36UL + (x))

size_t RELZ4_compress(const void *src, size_t srcSize, void *out);
size_t RELZ4_decompress(const void *src, size_t srcSize, void *out);

#ifdef __cplusplus
}
#endif
