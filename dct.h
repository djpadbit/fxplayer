#ifndef __DCT_H__
#define __DCT_H__

#include <stdint.h>

// 2D DCTs need to be divided by 8 afterwards (>>3)
void dct8(int32_t *vector, uint32_t stride);
void idct8(int32_t *vector, uint32_t stride);
void dct8x8(int32_t *vector);
void idct8x8(int32_t *vector);

#endif