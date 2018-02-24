#include "bitmap.h"

// converts a block index to an index in the array,
// and a the offset of the bit inside the array
BitMapEntryKey BitMap_blockToIndex(int num){
  BitMapEntryKey entry;
  //we get the entry of the bit map which contains the info about the block
  entry.entry_num=num/8;
  // we get the displacement inside the block to locate the bit we need
  entry.bit_num=num%8;
  return entry;
}

// converts a bit to a linear index
int BitMap_indexToBlock(int entry, uint8_t bit_num){
  return entry*8+bit_num;
}

// returns the index of the first bit having status "status"
// in the bitmap bmap, and starts looking from position start
// it returns -1 if it's not found
int BitMap_get(BitMap* bmap, int start, int status){
  int i,j,found=0;
  int entry=start/8;
  int bit=start%8;
  uint8_t mask;
  for(i=entry;i<bmap->num_blocks && !found;i++){
    mask=128;
    for(j=bit;j<8 && bit<bmap->num_bits && !found;j++){
      found=((mask & bmap->entries[i])?1:0)==status;
      if(!found){
        bit++;
        mask=mask>>1;
      }
    }
  }
  if(found){
    return bit;
  }
  return -1;
}

// sets the bit at index pos in bmap to status
int BitMap_set(BitMap* bmap, int pos, int status){
  int i,entry=(pos-1)/8, bit=(pos-1)%8;
  uint8_t mask=128;

  for(i=0;i<bit;i++){
    mask>>=1;
  }
  bmap->entries[entry]= (status)? bmap->entries[entry] | mask : bmap->entries[entry] & ~mask;

  return status;
}
