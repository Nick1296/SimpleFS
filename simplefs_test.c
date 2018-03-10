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

//mode=0 writes on all blocks and and a non existent once
//mode=1 read and frees all blocks
void driver_test(DiskDriver *disk, int mode){

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
    char v[513] = "<Test di scrittura su settore del nostro bellissimo disk_driver, devo scrivere tanto per provare il corretto funzionamento.hjefgkjhgfwsgfkgfwsejkfgshfglsujgfjkhgfjushegjkhdsgfjlhsgfvjlhdfjgyflyfgaelfialrgigsdiufgdriyihulkfuilfuhwlidiujelkifudlgdfjlvshgdshfgsdahjsfgsuyhgkjuddsdydsgflysdfydsgflydgljsgjhshgvfdljhgslhkgdkfvglhgsdlhfghdslgsajhbsdjvfgshfsdhvgsdfklgsjdgfsdhubggfjsdfgsdhjfvgsjgfgsdjhfgjsahfdfgsdilvcfsdgfvckhdsvgcbsdghfgslhcvlhgfdsjhbfhfgdsxlflvkhfgdfhsdbglgdflhegfjlhsagflhjegbfls<dgderlhfgbvs Test>";
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

void diskdriver_test(DiskDriver* disk){

  printf("\t\t\t Testing disk driver module:\n");
  printf("\t\t Testing disk_init:\n");
  //let's eliminate the previously created file if any
  unlink("disk");
  //we create a new disk
  DiskDriver_init(disk,"disk",10);
  printf("associated file descriptor: %d\n",disk->fd);
  driver_test(disk, 0);
  //bitmap_test(&disk);
  printf("disk drive shutdown\n");
  DiskDriver_shutdown(disk);
  printf("\t\t Testing disk_load:\n");
  //now let's retest all the functions with an existing file
  DiskDriver_load(disk,"disk",10);
  driver_test(disk, 1);
  //bitmap_test(&disk);
  printf("disk drive shutdown...");
  DiskDriver_shutdown(disk);
  printf("done.\n");
}

void format_test(SimpleFS* fs){
  printf("\t\t Testing SimpleFS_format:\n");
  SimpleFS_format(fs);
  driver_test(fs->disk, 0);
  DiskDriver_shutdown(fs->disk);
  DiskDriver_load(fs->disk,fs->filename,10);
  driver_test(fs->disk, 1);
  DiskDriver_shutdown(fs->disk);
}

void init_test(SimpleFS *fs){
  DirectoryHandle *dh;
  int res;
  printf("\t\t Testing SimpleFS_init:\n");
  SimpleFS_format(fs);
  res=DiskDriver_load(fs->disk,fs->filename,fs->block_num);
  CHECK_ERR(res==FAILED,"can't load the fs");
  dh=SimpleFS_init(fs,fs->disk);
  printf("now we print the directory handle\n");
  printf("simplefs: %p\n",dh->sfs);


  printf("current_block: %p\n",dh->current_block);
  printf("\t->prev block: %d\n",dh->current_block->previous_block);
  printf("\t->next block: %d\n",dh->current_block->next_block);
  printf("\t->block in file: %d\n",dh->current_block->block_in_file);

  printf("FirstDirectoryBlock: %p\n",dh->dcb);

  printf("\t->FCB:\n");
  printf("\t\t->parent directory: %d\n",dh->dcb->fcb.directory_block);
  printf("\t\t->name: %s\n",dh->dcb->fcb.name);
  printf("\t\t->size in bytes: %d\n",dh->dcb->fcb.size_in_bytes);
  printf("\t\t->size in blocks: %d\n",dh->dcb->fcb.size_in_blocks);
  printf("\t\t->is dir: %d\n",dh->dcb->fcb.is_dir);

  printf("\t->num entries: %d\n",dh->dcb->num_entries);


  printf("parent directory: %p\n",dh->directory);
  printf("pos in block: %d\n",dh->pos_in_block);
  printf("pos in dir: %d\n",dh->pos_in_dir);
}
void create_test(DirectoryHandle* dh){
  //firstly we create try to create two file with the same name to test if they are detected as files with the same name
	printf("creating a file with 128 charaters name\n");
	int i;
	char fname[128];
	for(i=0;i<127;i++){
		fname[i]='a';
	}
	fname[127]='\0';
	FileHandle* fh=SimpleFS_createFile(dh,fname);
	printf("result: %p\n",fh);
	//cleanup of the allocated memory
	free(fh->fcb);
	free(fh);
	printf("we create a file with name: test\n");
	fh=SimpleFS_createFile(dh,"test");
	printf("result: %p\n",fh);
	//cleanup of the allocated memory
	free(fh->fcb);
	free(fh);
  printf("creating a new file named test, for the 2nd time\n");
	fh=SimpleFS_createFile(dh,"test");
	printf("result: %p\n",fh);

	//then we create files to test if the directory block are allocated successfully
  char name[4];
  for(i=0;i<10;i++){
    sprintf(name, "%d",i);
    fh=SimpleFS_createFile(dh,name);
		printf("result: %p\n",fh);
		//cleanup of the allocated memory
		free(fh->fcb);
		free(fh);
  }
}

int main(void) {
  int res;
  unlink("disk");
  DiskDriver *disk=(DiskDriver*)malloc(sizeof(DiskDriver));
  SimpleFS *fs=(SimpleFS*)malloc(sizeof(SimpleFS));
  char diskname[]="disk";
  fs->block_num=30;
  fs->filename=diskname;
  fs->disk=disk;

  SimpleFS_format(fs);
  res=DiskDriver_load(fs->disk,fs->filename,fs->block_num);
  CHECK_ERR(res==FAILED,"can't load the fs");
  DirectoryHandle *dh=SimpleFS_init(fs,disk);
  create_test(dh);

	DiskDriver_shutdown(disk);
	free(disk);
	free(dh->dcb);
	free(dh);
	free(fs);
}
