#pragma once

typedef struct _BitMap {
	int num_bits;
	int num_blocks;
	uint8_t *entries;
} BitMap;

typedef struct _BitMapEntryKey {
	int entry_num;
	uint8_t bit_num;
} BitMapEntryKey;