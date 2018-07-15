#include "bitmap.h"
#include "common.h"

void BitMap_init(BitMap *b, int bitmap_blocks, int disk_blocks, int occupation) {
	//initializing the bitmap
	b->entries = (uint8_t *) b + sizeof(BitMap);
	b->num_blocks = bitmap_blocks;
	b->num_bits = disk_blocks;
	uint8_t *bitmap = b->entries;
	// we use a mask to sign the occupied blocks
	uint8_t mask;
	int j, i, write = occupation, written = 0;
	for (i = 0; i < bitmap_blocks; i++) {
		mask = 0;
		for (j = 0; j < 8 && written < write; j++) {
			mask = mask >> 1;
			mask += 128;
			written++;
		}
		bitmap[i] = mask;
	}
}


// converts a block index to an index in the array,
// and a the offset of the bit inside the array
BitMapEntryKey BitMap_blockToIndex(int num) {
	BitMapEntryKey entry;
	//we get the entry of the bit map which contains the info about the block
	entry.entry_num = num / 8;
	// we get the displacement inside the block to locate the bit we need
	entry.bit_num = (uint8_t) (num % 8);
	return entry;
}

// converts a bit to a linear index
int BitMap_indexToBlock(int entry, uint8_t bit_num) {
	return entry * 8 + bit_num;
}

// returns the index of the first bit having status "status"
// in the bitmap bmap, and starts looking from position start
// it returns -1 if it's not found
int BitMap_get(BitMap *bmap, int start, int status) {
	int i, j, found = 0;
	int entry = start / 8;
	// bit that have been already checked/skipped
	int checked = start;
	//initial displacement in the entry
	int starting_bit;
	uint8_t mask;

	for (i = entry; i < bmap->num_blocks && !found; i++) {
		starting_bit = checked % 8;
		mask = 128;
		mask = mask >> starting_bit;
		for (j = starting_bit; j < 8 && checked < bmap->num_bits && !found; j++) {
			found = ((mask & bmap->entries[i]) ? 1 : 0) == status;
			if (!found) {
				checked++;
				mask = mask >> 1;
			}
		}
	}
	if (found) {
		return checked;
	}
	return FAILED;
}

// sets the bit at index pos in bmap to status
int BitMap_set(BitMap *bmap, int pos, int status) {
	int entry = (pos) / 8, bit = (pos) % 8;
	uint8_t mask = 128;

	mask = mask >> bit;
	bmap->entries[entry] = (status) ? bmap->entries[entry] | mask : bmap->entries[entry] & ~mask;

	return status;
}

// return the status of the bit at index pos in bmap
int BitMap_test(BitMap *bmap, int pos) {
	int entry = pos / 8;
	int bit = pos % 8;
	uint8_t mask = 128;
	mask = mask >> bit;
	return (bmap->entries[entry] & mask) ? 1 : 0;
}
