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
#include "dct.h"

#define mprint(x,y,fmt, ...) dprint((x)*6-5, (y)*8-8, C_BLACK, fmt,  ##__VA_ARGS__)

const char *infile = "data.bin";

#define DC_CHUNK_SIZE 8
#define DC_CHUNK_BITS 7
#define DC_WIDTH (DWIDTH/DC_CHUNK_SIZE)
#define DC_HEIGHT (DHEIGHT/DC_CHUNK_SIZE)

#define STREAM_TEMP_SIZE 1024

#define DITHERING
#define FLOYD_DITHERING
#define BAYER_WIDTH 4
#define BAYER_HEIGHT 2

uint8_t __attribute__((section (".xram"))) framebuffer[DWIDTH*DHEIGHT];
uint32_t __attribute__((section (".yram"))) temp_dct[64];

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

huffman_tree huff_tree, huff_tree_val;

uint16_t quantmatrix[8*8];

uint8_t zigzag[64] = {0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,
					42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};

static void read_dct(int32_t *output, bitstream *bs)
{
	memset(output,0,64*sizeof(int32_t));
	int8_t dcval = read_huffman(&huff_tree_val,bs);
	output[0] = dcval*quantmatrix[0];
	for (int i=1;i<64;) {
		uint8_t val = read_huffman(&huff_tree,bs);
		if (!val)
			break;
		i += val >> 4;
		uint8_t nbval = val & 0xF;
		if (!nbval)
			continue;
		int8_t symb = read_huffman(&huff_tree_val,bs);
		for (int rep=0;rep<nbval;rep++) {
			uint8_t idx = zigzag[i++];
			output[idx] = symb*quantmatrix[idx];
		}
	}
	idct8x8(output);
}

static inline int32_t clip(int32_t val, int32_t low, int32_t high)
{
	if (val < low) return low;
	if (val > high) return high;
	return val;
}

static void write_dct(int32_t *dct, uint8_t x, uint8_t y)
{
	uint8_t xdst = x+DC_CHUNK_SIZE;
	uint8_t ydst = y+DC_CHUNK_SIZE;
	int pos = 0;
	for (;y<ydst;y++) {
		for (uint8_t xf = x;xf<xdst;xf++) {
			framebuffer[(y*DWIDTH)+xf] = 255-(uint8_t)(clip((dct[pos++]>>3)+128,0,255));
		}
	}
}

void decode_delta(bitstream *bs)
{
	uint16_t nb_chunks = read_bits(bs,DC_CHUNK_BITS)+1;
	//printf("Chunks %i\n", nb_chunks);
	for (uint16_t chunk=0;chunk<nb_chunks;chunk++) {
		uint16_t idx = read_bits(bs,DC_CHUNK_BITS);
		uint8_t x = (idx%DC_WIDTH)*DC_CHUNK_SIZE;
		uint8_t y = (idx/DC_WIDTH)*DC_CHUNK_SIZE;
		read_dct((int32_t*)&temp_dct,bs);
		write_dct((int32_t*)&temp_dct,x,y);
	}
}

void decode_huffman(bitstream *bs)
{
	for (uint8_t y=0;y<DHEIGHT;y+=DC_CHUNK_SIZE) {
		for (uint8_t x=0;x<DWIDTH;x+=DC_CHUNK_SIZE) {
			read_dct((int32_t*)&temp_dct,bs);
			write_dct((int32_t*)&temp_dct,x,y);
		}
	}
}

static inline uint8_t closest_value(uint8_t val)
{
	return (val > 127) ? 255 : 0;
}

/* // Gray
static inline void setpixel(int x, int y, uint8_t val)
{
	if (val > 189)
		return dpixel(x,y,C_BLACK);
	if (val > 126)
		return dpixel(x,y,C_DARK);
	if (val > 63)
		return dpixel(x,y,C_LIGHT);
}
*/

#ifndef DITHERING
void dither_framebuffer()
{
	for (int y=0;y<DHEIGHT;y++) {
		for (int x=0;x<DWIDTH;x++) {
			if (closest_value(framebuffer[(y*DWIDTH)+x]))
				dpixel(x,y,C_BLACK);
		}
	}
}
#else
#ifdef FLOYD_DITHERING

static inline int32_t clamp(int32_t val, int32_t min, int32_t max)
{
	if (val > max) return max;
	if (val < min) return min;
	return val;
}

void dither_framebuffer()
{
	int32_t quant_error = 0;
	for (int y=0;y<DHEIGHT;y++) {
		for (int x=0;x<DWIDTH;x++) {
			uint16_t idx = (y*DWIDTH)+x;
			uint8_t old_pix = framebuffer[idx];
			uint8_t new_pix = closest_value(old_pix);
			framebuffer[idx] = new_pix;
			quant_error = old_pix-new_pix;
			if (x != (DWIDTH-1))
				framebuffer[idx+1] = clamp(framebuffer[idx+1] + (quant_error * 7) / 16,0,255);
			if (y != (DHEIGHT-1)) {
				if (x != 0)
					framebuffer[idx+DWIDTH-1] = clamp(framebuffer[idx+DWIDTH-1] + (quant_error * 3) / 16,0,255);
				framebuffer[idx+DWIDTH] = clamp(framebuffer[idx+DWIDTH] + (quant_error * 5) / 16,0,255);
				if (x != (DWIDTH-1))
					framebuffer[idx+DWIDTH+1] = clamp(framebuffer[idx+DWIDTH+1] + quant_error / 16,0,255);
			}
			if (new_pix)
				dpixel(x,y,C_BLACK);
		}
	}
}
#else
uint8_t __attribute__((section (".yram"))) bayer_matrix[BAYER_WIDTH*BAYER_HEIGHT];

void init_bayer_matrix()
{
	int idx = 0;
	float coeff = 256/(BAYER_WIDTH*BAYER_HEIGHT);
	unsigned L = 0;
	unsigned M = 0;
	for (unsigned t=BAYER_HEIGHT;t;t>>=1) L++;
	for (unsigned t=BAYER_WIDTH;t;t>>=1) M++;
	L--;
	M--;
	for(unsigned y=0; y<BAYER_HEIGHT; ++y) {
		for(unsigned x=0; x<BAYER_WIDTH; ++x) {
			unsigned v = 0, offset=0, xmask = M, ymask = L;                         
			if(M==0 || (M > L && L != 0)) {
				unsigned xc = x ^ ((y << M) >> L), yc = y;
				for(unsigned bit=0; bit < M+L; ) {
					v |= ((yc >> --ymask)&1) << bit++;
					for(offset += M; offset >= L; offset -= L)
						v |= ((xc >> --xmask)&1) << bit++;
				}
			} else {   
				unsigned xc = x, yc = y ^ ((x << L) >> M);
				for(unsigned bit=0; bit < M+L; ) {
					v |= ((xc >> --xmask)&1) << bit++;
					for(offset += L; offset >= M; offset -= M)
						v |= ((yc >> --ymask)&1) << bit++;
				}
			}
			bayer_matrix[idx++] = (uint8_t)(v*coeff);
		}
	}
}
			
void dither_framebuffer()
{
	for (int y=0;y<DHEIGHT;y++) {
		int ybidx = BAYER_WIDTH*(y%BAYER_HEIGHT);
		for (int x=0;x<DWIDTH;x++) {
			uint8_t pix = framebuffer[(y*DWIDTH)+x];
			if (pix > bayer_matrix[ybidx+(x%BAYER_WIDTH)])
				dpixel(x,y,C_BLACK);
		}
	}
}
#endif
#endif

int decode_frame(bitstream *bs)
{
	uint16_t cmd = read_bits(bs,2);
	if (!cmd)
		return 2;
	switch (cmd) {
		case 3: // Duplicate frame
			break;
		case 2: // Delta frame
			decode_delta(bs);
			return 1;
		case 1: // Huffman frame
			decode_huffman(bs);
			return 1;
		case 0:
		default:
			return 2;
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

	for (int i=0;i<8*8;i++) {
		quantmatrix[i] = read_bits(&bs,16);
	}

	int ret = init_huffman(&huff_tree,&bs);
	int ret2 = init_huffman(&huff_tree_val,&bs);
	if (ret || ret2) {
		mprint(1,1,"Huffman tree");
		mprint(1,2,"coudln't be");
		mprint(1,3,"made, what ?");
		mprint(1,4,"ret = %i",ret);
		mprint(1,5,"ret2 = %i",ret2);
		dupdate();
		while (getkey().key != KEY_EXIT);
		close_stream(&stream);
		return 1;
	}

#ifdef DITHERING
#ifndef FLOYD_DITHERING
	init_bayer_matrix();
#endif
#endif

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

		ret = decode_frame(&bs);
		if (ret == 2)
			break;
		if (ret == 1) {
			dclear(C_WHITE);
			dither_framebuffer();
			dupdate();
		}

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