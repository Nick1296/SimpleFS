#include <stdio.h>
#include <unistd.h>
#include "simplefs.h"
#include <string.h>

void bitmap_info(DiskDriver *disk){
  int i,j,printed=0,print;
  uint8_t mask;
  printf("\t Bitmap info\n");
  print=disk->header->num_blocks;
  for(i=0;i<(disk->header->num_blocks+7)/8;i++){
    mask=128;
    printf("block %d:\n",i);
    for(j=0;j<8 && printed<print;j++){
      printf("%c",((mask & disk->bmap->entries[i])? '1' :'0'));
      mask=mask>>1;
      printed++;
    }
    printf("\n");
  }

}

void driver_test(DiskDriver *disk, int mode){

  DiskDriver_init(disk,"disk",10);
  printf("associated file descriptor: %d\n",disk->fd);

  printf("\t DiskHeader info:\n");
  printf("num blocks: %d\n",disk->header->num_blocks );
  printf("num bitmap blocks: %d\n",disk->header->bitmap_blocks );
  printf("bitmap entries: %d\n",disk->header->bitmap_entries );
  printf("num free blocks: %d\n",disk->header->free_blocks );
  printf("first free block: %d\n",disk->header->first_free_block );

  bitmap_info(disk);

  printf("\t testing getFreeBlock\n");

  int result, blockToWrite, i;
  for(i=0; i<disk->header->num_blocks && !mode; i++){
    // provare le altre funzioni
    blockToWrite = DiskDriver_getFreeBlock(disk, i);
    printf("disk_getFreeBlock result: %d\n", blockToWrite);

    printf("\t testing disk_write\n");
    char *v = "<Test di scrittura su settore del nostro bellissimo disk_driver, devo scrivere tanto per provare il corretto funzionamento.hjefgkjhgfwsgfkgfwsejkfgshfglsujgfjkhgfjushegjkhdsgfjlhsgfvjlhdfjgyflyfgaelfialrgigsdiufgdriyihulkfuilfuhwlidiujelkifudlgdfjlvshgdshfgsdahjsfgsuyhgkjuddsdydsgflysdfydsgflydgljsgjhshgvfdljhgslhkgdkfvglhgsdlhfghdslgsajhbsdjvfgshfsdhvgsdfklgsjdgfsdhubggfjsdfgsdhjfvgsjgfgsdjhfgjsahfdfgsdilvcfsdgfvckhdsvgcbsdghfgslhcvlhgfdsjhbfhfgdsxlflvkhfgdfhsdbglgdflhegfjlhsagflhjegbfls<dgderlhfgbvs Test>";
    result = DiskDriver_writeBlock(disk, v, blockToWrite);
    printf("disk_write result: %d\n", result);
  }

  printf("\t testing disk_flush\n");
  result = DiskDriver_flush(disk);
  printf("flush result: %d\n", result);

  if(mode) bitmap_info(disk);

  for(i=0; i<disk->header->num_blocks && mode; i++){
    printf("\t testing disk_read\n");
    char data[BLOCK_SIZE];
    result = DiskDriver_readBlock(disk, data, i);
    printf("disk_read block num %d, result: %d\n", i, result);
    printf("Data readed->%s<-end\n\n",data);
    memset(data, 0, sizeof(data));    
  }
  
  if(mode) printf("\n\n\t testing disk_freeBlock\n");

  for(i=disk->header->bitmap_blocks; i<disk->header->num_blocks && mode; i++){
    result = DiskDriver_freeBlock(disk, i);
    printf("libero il blocco: %d\nfreeBlock result: %d\n", i, result);

  }

  bitmap_info(disk);

  printf("\t testing disk_flush\n");
  result = DiskDriver_flush(disk);
  printf("flush result: %d\n", result);

  bitmap_info(disk);

}

void bitmap_test(DiskDriver *disk){
  printf("\t\t\t Testing bitmap module:\n");

  printf("\t\t Testing BitMap_blockToIndex:\n");
  BitMapEntryKey t1,t2;
  int b1=4,b2=11;
  t1=BitMap_blockToIndex(b1);
  printf("block: %d, entry_num: %d, bit_num: %d\n",b1,t1.entry_num,t1.bit_num);
  t2=BitMap_blockToIndex(b2);
  printf("block: %d, entry_num: %d, bit_num: %d\n",b2,t2.entry_num,t2.bit_num);

  printf("\t\t Testing BitMap_indexToBlock\n");
  b1=BitMap_indexToBlock(t1.entry_num,t1.bit_num);
  printf("entry:%d, bit_num:%d, block: %d\n",t1.entry_num,t1.bit_num,b1);
  b2=BitMap_indexToBlock(t2.entry_num,t2.bit_num);
  printf("entry:%d, bit_num:%d, block: %d\n",t2.entry_num,t2.bit_num,b2);

  printf("\t\t Testing BitMap_get\n");
  b1=BitMap_get(disk->bmap,6,0);
  printf("start:%d, status: %d, bit_num:%d\n",6,0,b1);
  b2=BitMap_get(disk->bmap,8,0);
  printf("start:%d, status:%d, bit_num:%d\n",8,0,b2);

  printf("\t\t Testing BitMap_test\n");
  b1=BitMap_test(disk->bmap,6);
  printf("status:%d, bit_num:%d\n",6,b1);
  b2=BitMap_test(disk->bmap,0);
  printf("status:%d,bit_num:%d\n",0,b2);

  printf("\t\t Testing BitMap_set\n");
  BitMap_set(disk->bmap,10,1);
  printf("status:%d, pos: %d\n",1,10);
  BitMap_set(disk->bmap,5,1);
  printf("status:%d, pos:%d\n",1,5);
  bitmap_info(disk);
  BitMap_set(disk->bmap,10,0);
  printf("status:%d, pos: %d\n",0,10);
  BitMap_set(disk->bmap,5,0);
  printf("status:%d, pos:%d\n",0,5);

  bitmap_info(disk);
}

int main(int agc, char** argv) {
  printf("FirstBlock size %ld\n", sizeof(FirstFileBlock));
  printf("DataBlock size %ld\n", sizeof(FileBlock));
  printf("FirstDirectoryBlock size %ld\n", sizeof(FirstDirectoryBlock));
  printf("DirectoryBlock size %ld\n", sizeof(DirectoryBlock));


  DiskDriver disk;
  printf("\t\t\t Testing disk driver module:\n");

  printf("\t\t Testing disk_init:\n");
  //let's eliminate the previously created file if any
  unlink("disk");
  driver_test(&disk, 0);
  //bitmap_test(&disk);
  printf("disk drive shutdown...");
  DiskDriver_shutdown(&disk);
  printf("done.\n");
  //now let's retest all the functions whit an existing file
  DiskDriver_load(&disk,"disk",10);
  driver_test(&disk, 1);
  //bitmap_test(&disk);
  printf("disk drive shutdown...");
  DiskDriver_shutdown(&disk);
  printf("done.\n");
}
