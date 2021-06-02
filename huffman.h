#ifndef __HUFFMAN_H__
#define __HUFFMAN_H__

#include <stdint.h>
#include "bitstream.h"

typedef struct huffman_node {
	struct huffman_node *parent;
	struct huffman_node *left,*right;
	uint8_t symbol;
	float frequency;
} huffman_node;

typedef struct {
	uint16_t nb_symbols;
	uint16_t nb_symbols_allocated;
	huffman_node *nodes;
	huffman_node *root;
} huffman_tree;

uint8_t read_huffman(huffman_tree *tree, bitstream *bs);
int init_huffman(huffman_tree *tree, bitstream *bs);
void cleanup_huffman(huffman_tree *tree);

#endif