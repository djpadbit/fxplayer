#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#ifdef SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_timer.h>
#define SCALE_FACTOR 4
#endif

#include "bitstream.h"
#include "huffman.h"

const char *outfile = "frames_dec.bin";

#define DWIDTH 128
#define DHEIGHT 64
#define DC_CHUNK_SIZE 4
#define DC_CHUNK_BITS 9
#define DC_WIDTH (DWIDTH/DC_CHUNK_SIZE)
#define DC_HEIGHT (DHEIGHT/DC_CHUNK_SIZE)

uint8_t framebuffer[(DWIDTH*DHEIGHT)/8];

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
	SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
	SDL_Rect rect;
	rect.w = SCALE_FACTOR;
	rect.h = SCALE_FACTOR;
	for (int y=0;y<DHEIGHT;y++) {
		for (int x=0;x<DWIDTH;x++) {
			uint8_t val = framebuffer[(y*(DWIDTH/8))+(x/8)];
			if ((val>>(7-(x%8)))&1) {
				rect.x = x*SCALE_FACTOR;
				rect.y = y*SCALE_FACTOR;
				SDL_RenderFillRect(rend, &rect);
			}
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

	if (init_huffman(&huff_tree,&bs)) {
		fprintf(stderr,"Couldn't create huffman tree\n");
		free(stream.data);
		return 1;
	}

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

	for (int frame=0;;frame++) {
		//printf("Frame %i ", frame);
		if (decode_frame(&bs))
			break;
		fwrite((uint8_t*)&framebuffer,(DWIDTH*DHEIGHT)/8,1,fptr);
#ifdef SDL
		if (show_frame(fpsf))
			break;
#endif
	}

#ifdef SDL
	close_sdl();
#endif

	cleanup_huffman(&huff_tree);

	free(stream.data);

	fclose(fptr);

	return 0;
}