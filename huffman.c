#include "huffman.h"

uint8_t read_huffman(huffman_tree *tree, bitstream *bs)
{
	/*if (!tree)
		return 0;*/
	huffman_node *node = tree->root;
	for (;;) {
		if (read_bits(bs,1))
			node = node->right;
		else
			node = node->left;
		if (!node->left && !node->right)
			return node->symbol;
	}
}

huffman_node *get_smallest_node(uint16_t nsymb, huffman_node *nodes)
{
	float freq = 1;
	huffman_node *node_small = NULL;
	for (int i=0;i<nsymb;i++) {
		huffman_node *node = &nodes[i];
		if (node->parent)
			continue;
		if (node->frequency < freq) {
			node_small = node;
			freq = node->frequency;
		}
	}
	return node_small;
}

int init_huffman(huffman_tree *tree, bitstream *bs)
{
	if (!tree)
		return 1;
	uint16_t nsymb = read_bits(bs,8)+1;
	uint16_t allocated = nsymb*2;
	huffman_node *nodes = (huffman_node*)calloc(sizeof(huffman_node),allocated);
	if (!nodes)
		return 1;

	for (int i=0;i<nsymb;i++) {
		huffman_node *node = &nodes[i];
		node->symbol = read_bits(bs,8);
		node->frequency = read_float(bs);
		node->parent = NULL;
		node->left = NULL;
		node->right = NULL;
	}

	uint16_t nb_nodes = nsymb;
	huffman_node *last_node = NULL;

	for (;;) {
		if (nb_nodes == allocated) {
			//printf("Fuck dude how does that happen ? %i/%i\n",nb_nodes,allocated);
			return 1;
		}
		huffman_node *nodem = &nodes[nb_nodes++];
		huffman_node *node1 = get_smallest_node(nb_nodes-1,nodes);
		if (!node1) {
			nb_nodes--;
			break;
		}
		node1->parent = nodem;
		huffman_node *node2 = get_smallest_node(nb_nodes-1,nodes);
		if (!node2) {
			node1->parent = NULL;
			node1->frequency = 1.0f;
			last_node = node1;
			nb_nodes--;
			break;	
		}
		node2->parent = nodem;
		nodem->frequency = node1->frequency + node2->frequency;
		nodem->symbol = 0;
		nodem->parent = NULL;
		nodem->left = node1;
		nodem->right = node2;
		last_node = nodem;
	}

	tree->nb_symbols = nsymb;
	tree->nb_symbols_allocated = allocated;
	tree->nodes = nodes;
	tree->root = last_node;
	return 0;
}

void cleanup_huffman(huffman_tree *tree)
{
	if (!tree)
		return;
	free(tree->nodes);
}