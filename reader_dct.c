#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#ifdef SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>
#define SCALE_FACTOR 8
#endif

#include "bitstream.h"
#include "huffman.h"
#include "dct.h"

const char *outfile = "frames_dec.bin";

#define DWIDTH 128
#define DHEIGHT 64
#define DC_CHUNK_SIZE 8
#define DC_CHUNK_BITS 7
#define DC_WIDTH (DWIDTH/DC_CHUNK_SIZE)
#define DC_HEIGHT (DHEIGHT/DC_CHUNK_SIZE)

uint8_t framebuffer[DWIDTH*DHEIGHT];

typedef struct {
	uint8_t *data;
	size_t size,ptr;
} stream;

uint8_t next_byte(void *stv)
{
	stream *st = (stream*)stv;
	if (st->ptr == st->size) return 0;
	return st->data[st->ptr++];
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
			framebuffer[(y*DWIDTH)+xf] = (uint8_t)(clip((dct[pos++]>>3)+128,0,255));
		}
	}
}

void decode_delta(bitstream *bs)
{
	uint16_t nb_chunks = read_bits(bs,DC_CHUNK_BITS)+1;
	//printf("Chunks %i\n", nb_chunks);
	int32_t dctout[64];
	for (uint16_t chunk=0;chunk<nb_chunks;chunk++) {
		uint16_t idx = read_bits(bs,DC_CHUNK_BITS);
		uint8_t x = (idx%DC_WIDTH)*DC_CHUNK_SIZE;
		uint8_t y = (idx/DC_WIDTH)*DC_CHUNK_SIZE;
		read_dct((int32_t*)&dctout,bs);
		write_dct((int32_t*)&dctout,x,y);
	}
}

void decode_huffman(bitstream *bs)
{
	int32_t dctout[64];
	for (uint8_t y=0;y<DHEIGHT;y+=DC_CHUNK_SIZE) {
		for (uint8_t x=0;x<DWIDTH;x+=DC_CHUNK_SIZE) {
			read_dct((int32_t*)&dctout,bs);
			write_dct((int32_t*)&dctout,x,y);
		}
	}
}

int decode_frame(bitstream *bs)
{
	uint16_t cmd = read_bits(bs,2);
	//printf("is %i\n",cmd);
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

#ifdef SDL

SDL_Window *win;
SDL_Renderer* rend;

int init_sdl()
{
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) { 
		printf("error initializing SDL: %s\n", SDL_GetError()); 
		return 1;
	} 
	win = SDL_CreateWindow("reader",
							SDL_WINDOWPOS_CENTERED, 
							SDL_WINDOWPOS_CENTERED, 
							DWIDTH*SCALE_FACTOR, DHEIGHT*SCALE_FACTOR, 0);
	rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	return 0;
}

int show_frame(float fps)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) { 
		switch (event.type) {
			case SDL_QUIT:
				return 1;
			default:
				break;
		}
	}

	SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);
	SDL_RenderClear(rend);
	SDL_Rect rect;
	rect.w = SCALE_FACTOR;
	rect.h = SCALE_FACTOR;
	for (int y=0;y<DHEIGHT;y++) {
		for (int x=0;x<DWIDTH;x++) {
			uint8_t val = framebuffer[(y*DWIDTH)+x];
			SDL_SetRenderDrawColor(rend, val, val, val, 255);
			rect.x = x*SCALE_FACTOR;
			rect.y = y*SCALE_FACTOR;
			SDL_RenderFillRect(rend, &rect);
		}
	}
	SDL_RenderPresent(rend);
	SDL_Delay(1000 / fps);
	return 0;
}

void close_sdl()
{
	SDL_DestroyRenderer(rend);
	SDL_DestroyWindow(win);
	SDL_Quit();
}

#endif

int main(int argc, char const *argv[])
{
	if (argc < 2) {
		printf("Give file path\n");
		return 1;
	}

	FILE *fptr = fopen(argv[1],"rb");
	if (!fptr) {
		fprintf(stderr,"Couldn't open the input file\n");
		return 1;
	}

	stream stream;

	fseek(fptr,0,SEEK_END);
	stream.size = ftell(fptr);
	fseek(fptr,0,SEEK_SET);

	stream.data = malloc(stream.size);
	stream.ptr = 0;
	if (!stream.data) {
		fprintf(stderr,"Couldn't allocate space for input file\n");
		fclose(fptr);
		return 1;
	}

	fread(stream.data,stream.size,1,fptr);

	fclose(fptr);

	fptr = fopen(outfile,"wb");
	if (!fptr) {
		fprintf(stderr,"Couldn't open the output file\n");
		free(stream.data);
		return 1;
	}

	bitstream bs;
	init_bitstream(&bs,&stream);
	uint16_t fps = read_bits(&bs,16);
	float fpsf = fps/1000;

	for (int i=0;i<8*8;i++) {
		quantmatrix[i] = read_bits(&bs,16);
	}

	if (init_huffman(&huff_tree,&bs)) {
		fprintf(stderr, "Couldn't create huffman tree\n");
	}

	if (init_huffman(&huff_tree_val,&bs)) {
		fprintf(stderr, "Couldn't create huffman tree\n");
	}

	printf("%i Symbols for markers (%i allocated)\n", huff_tree.nb_symbols, huff_tree.nb_symbols_allocated);
	printf("%i Symbols for values (%i allocated)\n", huff_tree_val.nb_symbols, huff_tree_val.nb_symbols_allocated);

	/*for (int i=0;i<nb_symbols;i++) {
		huffman_node *old_node = &nodes[i];
		printf("%i ", old_node->symbol);
		for (;;) {
			huffman_node *node = old_node->parent;
			if (node->left == old_node) {
				printf("0");
			} else {
				printf("1");
			}
			if (!node->parent)
				break;
			old_node = node;
		}
		printf("\n");
	}

	for (huffman_node *node=huffman_root;;) {
		if (!node)
			break;
		printf("Node\n");
		printf(" -Symbol %i\n", node->symbol);
		printf(" -Frequency %f\n", node->frequency);
		printf(" -Parent %x\n", node->parent);
		printf(" -Left %x\n", node->left);
		printf(" -Right %x\n", node->right);
		node = node->left;
	}*/

#ifdef SDL
	init_sdl();
#endif

	for (int frame=1;;frame++) {
		//printf("Frame %i ", frame);
		if (decode_frame(&bs))
			break;
		fwrite((uint8_t*)&framebuffer,DWIDTH*DHEIGHT,1,fptr);
#ifdef SDL
		if (show_frame(fpsf))
			break;
#endif
	}

#ifdef SDL
	close_sdl();
#endif

	cleanup_huffman(&huff_tree);
	cleanup_huffman(&huff_tree_val);

	free(stream.data);

	fclose(fptr);

	return 0;
}