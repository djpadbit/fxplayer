#ifndef __BITSTREAM_H__
#define __BITSTREAM_H__

// stdlib for LITTLE_ENDIAN
#ifndef FX9860G
#include <stdlib.h>
#else
#include <gint/std/stdlib.h>
#endif
#include <stdint.h>

typedef struct {
	union {
		uint32_t data;
		uint16_t shorts[2];
		uint8_t bytes[4];
	};
	uint8_t available;
	void *stream;
} bitstream;

extern uint8_t next_byte(void *stream);
uint8_t feed_byte(bitstream *bs, uint8_t byte);
uint32_t read_bits(bitstream *bs, uint8_t nb);
void init_bitstream(bitstream *bs, void *stream);
float read_float(bitstream *bs);

#endif