#include "bitstream.h"

uint8_t feed_byte(bitstream *bs, uint8_t byte)
{
	if (32-bs->available < 8)
		return 0;
#ifdef LITTLE_ENDIAN
	uint8_t bytenb = 3-(bs->available/8);
#else
	uint8_t bytenb = bs->available/8;
#endif
	uint8_t bytemod = bs->available%8;
	if (bytemod == 0) {
		bs->bytes[bytenb] = byte;
	} else {
		bs->bytes[bytenb] |= byte>>bytemod;
#ifdef LITTLE_ENDIAN
		bs->bytes[bytenb-1] |= byte<<(8-bytemod);
#else
		bs->bytes[bytenb+1] |= byte<<(8-bytemod);
#endif
	}
	bs->available += 8;
	return 1;
}

// Always able to read at most 24bits
uint32_t read_bits(bitstream *bs, uint8_t nb)
{
	uint32_t out = bs->data>>(32-nb);
	bs->data<<=nb;
	bs->available -= nb;
	while (32-bs->available >= 8)
		feed_byte(bs,next_byte(bs->stream));
	return out;
}

void init_bitstream(bitstream *bs, void *st)
{
	bs->data = 0;
	bs->available = 0;
	bs->stream = st;
	for (int i=0;i<4;i++)
		feed_byte(bs,next_byte(st));
}

float read_float(bitstream *bs)
{
//#ifdef LITTLE_ENDIAN
	uint32_t f = read_bits(bs,8)|read_bits(bs,8)<<8|read_bits(bs,8)<<16|read_bits(bs,8)<<24;
/*#else
	uint32_t f = read_bits(bs,24)<<8|read_bits(bs,8);
#endif*/
	return *((float*)&f);
}
