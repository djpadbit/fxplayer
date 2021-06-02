/* 
 * Fast discrete cosine transform algorithms (C)
 * 
 * Copyright (c) 2017 Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/fast-discrete-cosine-transform-algorithms
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */

#include "dct.h"

#define INDEX(val) (val*stride)

// https://fgiesen.wordpress.com/2013/11/04/bink-2-2-integer-dct-design-part-1/

void dct8(int32_t *vector, uint32_t stride)
{
	// extract rows
	int32_t i0 = vector[INDEX(0)];
	int32_t i1 = vector[INDEX(1)];
	int32_t i2 = vector[INDEX(2)];
	int32_t i3 = vector[INDEX(3)];
	int32_t i4 = vector[INDEX(4)];
	int32_t i5 = vector[INDEX(5)];
	int32_t i6 = vector[INDEX(6)];
	int32_t i7 = vector[INDEX(7)];

	// stage 1 - 8A
	int32_t a0 = i0 + i7;
	int32_t a1 = i1 + i6;
	int32_t a2 = i2 + i5;
	int32_t a3 = i3 + i4;
	int32_t a4 = i0 - i7;
	int32_t a5 = i1 - i6;
	int32_t a6 = i2 - i5;
	int32_t a7 = i3 - i4;

	// even stage 2 - 4A
	int32_t b0 = a0 + a3;
	int32_t b1 = a1 + a2;
	int32_t b2 = a0 - a3;
	int32_t b3 = a1 - a2;

	// even stage 3 - 6A 4S
	int32_t c0 = b0 + b1;
	int32_t c1 = b0 - b1;
	int32_t c2 = b2 + (b2>>2) + (b3>>1);
	int32_t c3 = (b2>>1) - b3 - (b3>>2);

	// odd stage 2 - 12A 8S
	// NB a4/4 and a7/4 are each used twice, so this really is 8 shifts, not 10.
	int32_t t1 = a7>>2;
	int32_t t2 = a4>>2;
	int32_t b4 = t1 + a4 + t2 - (a4>>4);
	int32_t b7 = t2 - a7 - t1 + (a7>>4);
	int32_t b5 = a5 + a6 - (a6>>2) - (a6>>4);
	int32_t b6 = a6 - a5 + (a5>>2) + (a5>>4);

	// odd stage 3 - 4A
	int32_t c4 = b4 + b5;
	int32_t c5 = b4 - b5;
	int32_t c6 = b6 + b7;
	int32_t c7 = b6 - b7;

	// odd stage 4 - 2A
	int32_t d4 = c4;
	int32_t d5 = c5 + c7;
	int32_t d6 = c5 - c7;
	int32_t d7 = c6;

	// permute/output
	vector[INDEX(0)] = c0;
	vector[INDEX(1)] = d4;
	vector[INDEX(2)] = c2;
	vector[INDEX(3)] = d6;
	vector[INDEX(4)] = c1;
	vector[INDEX(5)] = d5;
	vector[INDEX(6)] = c3;
	vector[INDEX(7)] = d7;

	// total: 36A 12S
}

void idct8(int32_t *vector, uint32_t stride)
{
	// extract rows (with input permutation)
	int32_t c0 = vector[INDEX(0)];
	int32_t d4 = vector[INDEX(1)];
	int32_t c2 = vector[INDEX(2)];
	int32_t d6 = vector[INDEX(3)];
	int32_t c1 = vector[INDEX(4)];
	int32_t d5 = vector[INDEX(5)];
	int32_t c3 = vector[INDEX(6)];
	int32_t d7 = vector[INDEX(7)];

	// odd stage 4
	int32_t c4 = d4;
	int32_t c5 = d5 + d6;
	int32_t c7 = d5 - d6;
	int32_t c6 = d7;

	// odd stage 3
	int32_t b4 = c4 + c5;
	int32_t b5 = c4 - c5;
	int32_t b6 = c6 + c7;
	int32_t b7 = c6 - c7;

	// even stage 3
	int32_t b0 = c0 + c1;
	int32_t b1 = c0 - c1;
	int32_t b2 = c2 + (c2>>2) + (c3>>1);
	int32_t b3 = (c2>>1) - c3 - (c3>>2);

	// odd stage 2
	int32_t t1 = b7>>2;
	int32_t t2 = b4>>2;
	int32_t a4 = t1 + b4 + t2 - (b4>>4);
	int32_t a7 = t2 - b7 - t1 + (b7>>4);
	int32_t a5 = b5 - b6 + (b6>>2) + (b6>>4);
	int32_t a6 = b6 + b5 - (b5>>2) - (b5>>4);

	// even stage 2
	int32_t a0 = b0 + b2;
	int32_t a1 = b1 + b3;
	int32_t a2 = b1 - b3;
	int32_t a3 = b0 - b2;

	// stage 1
	// output
	vector[INDEX(0)] = a0 + a4;
	vector[INDEX(1)] = a1 + a5;
	vector[INDEX(2)] = a2 + a6;
	vector[INDEX(3)] = a3 + a7;
	vector[INDEX(4)] = a3 - a7;
	vector[INDEX(5)] = a2 - a6;
	vector[INDEX(6)] = a1 - a5;
	vector[INDEX(7)] = a0 - a4;
}

void dct8x8(int32_t *vector)
{
	for (int i=0;i<8;i++)
		dct8(&vector[i*8],1);
	for (int i=0;i<8;i++)
		dct8(&vector[i],8);
}

void idct8x8(int32_t *vector)
{
	for (int i=0;i<8;i++)
		idct8(&vector[i*8],1);
	for (int i=0;i<8;i++)
		idct8(&vector[i],8);
}