#pragma once

#include "bitmap_structures.h"

// this is stored in the 1st block of the disk
typedef struct _DiskHeader {
	int num_blocks;      // blocks used to store files and directories
	int bitmap_blocks;   // how many blocks used by the bitmap
	int bitmap_entries;  // how many bytes are needed to store the bitmap
	
	int free_blocks;     // free blocks
	int first_free_block;// first block index
} DiskHeader;

typedef struct _DiskDriver {
	DiskHeader *header; // mmapped
	BitMap *bmap;       // mmapped (bitmap)
	int fd;             // for us
	// Manca il puntatore di dove iniziano i blocchi per scrivere i dati
} DiskDriver;

/**
   The blocks indices seen by the read/write functions
   have to be calculated after the space occupied by the bitmap
*/