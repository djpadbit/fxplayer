#include <gint/std/stdio.h>
#include <gint/std/stdlib.h>
#include <gint/std/string.h>
#include <gint/keyboard.h>
#include <gint/display.h>
#include <gint/rtc.h>
#include <gint/timer.h>
#include <gint/bfile.h>
#include <gint/clock.h>
#include <stdint.h>

#include "bitstream.h"
#include "huffman.h"

#define mprint(x,y,fmt, ...) dprint((x)*6-5, (y)*8-8, C_BLACK, fmt,  ##__VA_ARGS__)

const char *infile = "data.bin";

#define DC_CHUNK_SIZE 4
#define DC_CHUNK_DATA_BITS (DC_CHUNK_SIZE*DC_CHUNK_SIZE)
#define DC_CHUNK_BITS 9
#define DC_WIDTH (DWIDTH/DC_CHUNK_SIZE)
#define DC_HEIGHT (DHEIGHT/DC_CHUNK_SIZE)

#define STREAM_TEMP_SIZE 1024

uint8_t *framebuffer;

typedef struct {
	int fd;
	size_t size,ptr;
	uint8_t *data;
	size_t tptr;
} stream;

uint8_t next_byte(void *stv)
{
	stream *st = (stream*)stv;
	if (st->ptr+st->tptr == st->size)
		return 0;
	if (st->tptr == STREAM_TEMP_SIZE) {
		st->ptr += STREAM_TEMP_SIZE;
		BFile_Read(st->fd,st->data,STREAM_TEMP_SIZE,st->ptr);
		st->tptr = 0;
	}
	return st->data[st->tptr++];
}

huffman_tree huff_tree;

void decode_delta(bitstream *bs) // Kind of hard coded for 4 chunk size
{
	uint16_t nb_chunks = read_bits(bs,DC_CHUNK_BITS)+1;
	//printf("Chunks %i\n", nb_chunks);
	for (uint16_t chunk=0;chunk<nb_chunks;chunk++) {
		uint16_t idx = read_bits(bs,DC_CHUNK_BITS);
		uint16_t data = read_huffman(&huff_tree,bs)<<8|read_huffman(&huff_tree,bs);
		uint8_t x = (idx%DC_WIDTH)*DC_CHUNK_SIZE;
		uint8_t y = (idx/DC_WIDTH)*DC_CHUNK_SIZE;
		uint16_t fidx = (y*(DWIDTH/8))+(x/8);
		uint8_t bits = x%8;
		for (int8_t ry=DC_CHUNK_SIZE-1;ry>=0;ry--) { // Chunk size should always be powers of 2, otherwise breaks
			if (bits == 0) // (data>>((ry-1)*DC_CHUNK_SIZE))&0xF0;
				framebuffer[fidx] = (framebuffer[fidx] & 0x0F) | (((data>>(ry*DC_CHUNK_SIZE))<<DC_CHUNK_SIZE)&0xF0);
			else if (bits == 4) 
				framebuffer[fidx] = (framebuffer[fidx] & 0xF0) | ((data>>(ry*DC_CHUNK_SIZE))&0xF);
			fidx += (DWIDTH/8);
		}
	}
}

void decode_huffman(bitstream *bs)
{
	uint16_t wrote = 0;
	uint8_t x = 0;
	uint8_t y = 0;
	while (wrote != DWIDTH*DHEIGHT) {
		uint8_t data = read_huffman(&huff_tree,bs);
		uint16_t fidx = (y*(DWIDTH/8))+(x/8);
		uint8_t bits = x%8;
		for (int8_t ry=2-1;ry>=0;ry--) {
			if (bits == 0)
				framebuffer[fidx] = (framebuffer[fidx] & 0x0F) | (((data>>(ry*4))<<4)&0xF0);
			else if (bits == 4) 
				framebuffer[fidx] = (framebuffer[fidx] & 0xF0) | ((data>>(ry*4))&0xF);
			fidx += (DWIDTH/8);
		}
		x += 4;
		if (x == DWIDTH) {
			x = 0;
			y += 2;
		}
		wrote += 8;
	}
}

int decode_frame(bitstream *bs)
{
	uint16_t cmd = read_bits(bs,2);
	if (!cmd)
		return 1;
	switch (cmd) {
		case 3: // Duplicate frame
			break;
		case 2: // Delta frame
			decode_delta(bs);
			break;
		case 1: // Huffman frame
			decode_huffman(bs);
			break;
		case 0:
		default:
			return 1;
	}
	return 0;
}

void ctfc(char *src, uint16_t *dst)
{
	int i;
	for (i=0;src[i]!=0;i++) dst[i] = src[i];
	dst[i] = 0;
}

void mkpth(uint16_t *dst,char *root,char *fold,char *fn)
{
	char tp[2+strlen(root)+1+strlen(fold)+1+strlen(fn)+1]; // probably off by 1 or 2 bytes
	if(strlen(fold)==0) sprintf(tp,"\\\\%s\\%s",root,fn); //File path without folder
	else if(strlen(fn)==0) sprintf(tp,"\\\\%s\\%s",root,fn); //File path without file
	else sprintf(tp,"\\\\%s\\%s\\%s",root,fold,fn); //File path with file & folder
	ctfc((char*)&tp,dst);
}

int init_stream(stream *st, const char *filename)
{
	uint16_t path[40];
	mkpth((uint16_t*)&path,"fls0","",(char*)filename);
	st->fd = BFile_Open(path,BFile_ReadOnly);
	if (st->fd < 0)
		return 1;
	st->size = BFile_Size(st->fd);
	st->ptr = 0;
	st->tptr = 0;
	st->data = malloc(STREAM_TEMP_SIZE);
	if (!st->data) {
		BFile_Close(st->fd);
		return 1;
	}
	BFile_Read(st->fd,st->data,STREAM_TEMP_SIZE,0);
	return 0;
}

void close_stream(stream *st)
{
	if (st->data) free(st->data);
	if (st->fd >= 0) BFile_Close(st->fd);
}

int htimer;
unsigned long timertime = 0;

int timer_callback()
{
	timertime++;
	return TIMER_CONTINUE;
}

#define TIMER_FREQ 6000
#define TIMER_DELAY (1.0/(double)TIMER_FREQ)
#define TIMER_DELAY_US (TIMER_DELAY*1000000.0)

void timer_startup()
{
	uint32_t delay = timer_delay(2, TIMER_DELAY_US, TIMER_Pphi_4);
	htimer = timer_setup(2, delay, timer_callback);
	timer_start(htimer);
}

void timer_cleanup()
{
	timer_stop(htimer);
}

int main() 
{
	framebuffer = (uint8_t*)gint_vram;
	dclear(C_WHITE);

	stream stream;
	if (init_stream(&stream,infile)) {
		mprint(1,1,"Stream init failed");
		mprint(1,2,"File missing ?");
		dupdate();
		while (getkey().key != KEY_EXIT);
		return 1;
	}

	bitstream bs;
	init_bitstream(&bs,&stream);

	uint16_t fpsu = read_bits(&bs,16);
	double fps = fpsu/1000.0;
	double dinterval = (1.0/fps)/TIMER_DELAY;
	unsigned long interval = (unsigned long)(dinterval+0.5); // cheap ass rounding

	int ret = init_huffman(&huff_tree,&bs);
	if (ret) {
		mprint(1,1,"Huffman tree");
		mprint(1,2,"coudln't be");
		mprint(1,3,"made, what ?");
		mprint(1,4,"ret = %i",ret);
		dupdate();
		while (getkey().key != KEY_EXIT);
		close_stream(&stream);
		return 1;
	}
	timer_startup();

	unsigned long frametime_avg = 0;
	unsigned long frametime_min = TIMER_FREQ;
	unsigned long frametime_max = 0;

	int frames_over = 0;
	int frames;
	unsigned long start_time = timertime;
	for (frames=1;;frames++) {
		//printf("Frame %i\n", frame);
		unsigned long target_time = start_time+(interval*frames);
		unsigned long start_dec_time = timertime;

		if (decode_frame(&bs))
			break;

		dupdate();
		unsigned long frametime = timertime-start_dec_time;
		frametime_avg += frametime;
		if (frametime > frametime_max) frametime_max = frametime;
		else if (frametime < frametime_min) frametime_min = frametime;

		if (frametime >= interval) frames_over += 1;

		while (timertime<target_time) sleep();
	}

	close_stream(&stream);
	cleanup_huffman(&huff_tree);
	timer_cleanup();

	dclear(C_WHITE);
	mprint(1,1,"Stats breakdown:");
	mprint(1,2,"Avg: %i",frametime_avg/frames);
	mprint(1,3,"Min: %i",frametime_min);
	mprint(1,4,"Max: %i",frametime_max);
	mprint(1,5,"Interval: %i",interval);
	mprint(1,6,"Frames: %i",frames);
	mprint(1,7,"Frames Over: %i",frames_over);
	dupdate();
	while (getkey().key != KEY_EXIT);

	return 0;
}