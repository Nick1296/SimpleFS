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
  DiskDriver_load(disk,"disk");
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
  DiskDriver_load(fs->disk,fs->filename);
  driver_test(fs->disk, 1);
  DiskDriver_shutdown(fs->disk);
}

void init_test(SimpleFS *fs){
  DirectoryHandle *dh;
  int res;
  printf("\t\t Testing SimpleFS_init:\n");
  SimpleFS_format(fs);
  res=DiskDriver_load(fs->disk,fs->filename);
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
void createFile_openFile_closeFile_test(DirectoryHandle* dh){
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
	SimpleFS_close(fh);
	printf("we create a file with name: test\n");
	fh=SimpleFS_createFile(dh,"test");
	printf("result: %p\n",fh);
	//cleanup of the allocated memory
	SimpleFS_close(fh);
  printf("creating a new file named test, for the 2nd time\n");
	fh=SimpleFS_createFile(dh,"test");
	printf("result: %p\n",fh);

	//then we create files to test if the directory block are allocated successfully
	printf("now we create a bunch of files\n");
  char name[4];
  for(i=0;i<9;i++){
    sprintf(name, "%d",i);
    fh=SimpleFS_createFile(dh,name);
		printf("result: %p\n",fh);
		//cleanup of the allocated memory
		SimpleFS_close(fh);
  }

  fh=SimpleFS_openFile(dh,"test");
	printf("opened file \"test\": %p\n",fh);
	printf("closing the file named \"test\"...");
	SimpleFS_close(fh);
	printf("done\n");
}

void read_seek_write_test(DirectoryHandle* dh){
	printf("\t\t Testing SimpleFS_write/SimpleFS_seek/SimpleFS_read\n");
	FileHandle *f=SimpleFS_createFile(dh,"rsw");
	char data1[1024],data2[1024];
	int i,res=1,out=0;
	for(i=0;i<1024;i++){
		data1[i]=i;
	}
	printf("we read a void file\n");
	out=SimpleFS_read(f,data2,1024);
	printf("bytes read :%d\n",out);
	printf("we write a file with number from 0 to 1023 the we read it and check if it's correct\n");
	out=SimpleFS_write(f,data1,1024);
	printf("bytes written :%d\n",out);
	bitmap_info(f->sfs->disk);
	res=SimpleFS_seek(f,0);
	printf("seek result :%d\n",res);
	out=SimpleFS_read(f,data2,1024);
	printf("bytes read :%d\n",out);
	res=1;
	for(i=0;i<1024 && res;i++){
		res=(data1[i]==data2[i])?1:0;
	}
	printf("result :%d\n",res);
	for(i=0;i<1024;i++){
		data1[i]=0;
		data2[i]=0;
	}
	printf("now we move the cursor to 512 byte and we write and read the numbers from 0 to -511\n");
	res=SimpleFS_seek(f,512);
	printf("seek result :%d\n",res);
	for(i=0;i<1024;i++){
		data1[i]=-i;
	}
	out=SimpleFS_write(f,data1,512);
	printf("bytes written :%d\n",out);
	res=SimpleFS_seek(f,512);
	printf("seek result :%d\n",res);
	out=SimpleFS_read(f,data2,512);
	printf("bytes read :%d\n",out);
	res=1;
	for(i=0;i<512 && res;i++){
		res=(data1[i]==data2[i])?1:0;
	}
	printf("result :%d\n",res);
	for(i=0;i<1024;i++){
		data1[i]=0;
		data2[i]=0;
	}
	printf("now we write another 512 bytes at the end of the file\n");
	res=SimpleFS_seek(f,1024);
	printf("seek result :%d\n",res);
	out=SimpleFS_write(f,data1+512,512);
	printf("bytes written :%d\n",out);
	res=SimpleFS_seek(f,1024);
	printf("seek result :%d\n",res);
	out=SimpleFS_read(f,data2+512,512);
	printf("bytes read :%d\n",out);
	res=1;
	for(i=512;i<1024 && res;i++){
		res=(data1[i]==data2[i])?1:0;
	}
	printf("result :%d\n",res);
	printf("we write a the begining of the file numbers from 0 to 128\n");
	for(i=0;i<1024;i++){
		data1[i]=i;
		data2[i]=0;
	}
	res=SimpleFS_seek(f,0);
	printf("seek result :%d\n",res);
	out=SimpleFS_write(f,data1,128);
	printf("bytes written :%d\n",out);
	res=SimpleFS_seek(f,0);
	printf("seek result :%d\n",res);
	out=SimpleFS_read(f,data2,128);
	printf("bytes read :%d\n",out);
	res=1;
	for(i=0;i<1024 && res;i++){
		res=(data1[i]==data2[i])?1:0;
	}
	printf("result :%d\n",res);
	printf("now we try to seek at the end of the file to test if it returns -1\n");
	res=SimpleFS_seek(f,2048);
	printf("result :%d\n",res);
	SimpleFS_close(f);
}

void readDir_test(DirectoryHandle *dh, int i){
  char* list;
  int ret=SimpleFS_readDir(&list, dh);
  if(ret==FAILED) printf("Errore in readDir_test\n");
  printf("\nList directory %d:\n%s\n\n",i,list);
  free(list);
}

void readDir_changeDir_mkDir_remove_test(DirectoryHandle* dh){
  printf("\t\tSimpleFS_functions test\n");
  printf("SimpleFS_changeDir ..\n");
  int ret=SimpleFS_changeDir(dh, "..");
  if(ret==FAILED) printf("Errore in changeDir ..\n");
  readDir_test(dh,1);
  printf("SimpleFS_mkDir ciao\n");
  ret=SimpleFS_mkDir(dh, "ciao");
  if(ret==FAILED) printf("Errore in mkDir_test1\n");
  if(ret!=FAILED) readDir_test(dh,2);
  printf("SimpleFS_changeDir ciao\n");
  ret=SimpleFS_changeDir(dh, "ciao");
  if(ret==FAILED) printf("Errore in changeDir ciao\n");
  if(ret!=FAILED) readDir_test(dh,3);
  char name[4];
  sprintf(name, "%d",9);
  printf("SimpleFS_createFile 9\n");
  FileHandle* fh=SimpleFS_createFile(dh,name);
  SimpleFS_close(fh);
  readDir_test(dh,4);
  printf("SimpleFS_changeDir ..\n");
  ret=SimpleFS_changeDir(dh, "..");
  if(ret==FAILED) printf("Errore in changeDir ..\n");
  if(ret!=FAILED) readDir_test(dh,5);

  bitmap_info(dh->sfs->disk);

  printf("SimpleFS_changeDir ciao\n");
  ret=SimpleFS_changeDir(dh, "ciao");
  if(ret==FAILED) printf("Errore in changeDir ciao\n");
  if(ret!=FAILED) readDir_test(dh,6);
  printf("SimpleFS_remove 9\n");
  ret=SimpleFS_remove(dh, name);
  if(ret==FAILED) printf("Errore in remove 9\n");
  if(ret!=FAILED) readDir_test(dh,7);
  printf("SimpleFS_changeDir ..\n");
  ret=SimpleFS_changeDir(dh, "..");
  if(ret==FAILED) printf("Errore in changeDir ..\n");
  if(ret!=FAILED) readDir_test(dh,8);
  printf("SimpleFS_remove ciao\n");
  ret=SimpleFS_remove(dh, "ciao");
  if(ret==FAILED) printf("Errore in remove ciao\n");
  if(ret!=FAILED) readDir_test(dh,9);

  printf("SimpleFS_mkDir ciao\n");
  ret=SimpleFS_mkDir(dh, "ciao");
  if(ret==FAILED) printf("Errore in mkDir_test2\n");
  printf("SimpleFS_changeDir ciao\n");
  ret=SimpleFS_changeDir(dh, "ciao");
  if(ret==FAILED) printf("Errore in changeDir ciao\n");
  printf("SimpleFS_mkDir sotto ciao\n");
  ret=SimpleFS_mkDir(dh, "sotto ciao");
  sprintf(name, "%d",9);
  printf("SimpleFS_createFile 9\n");
  fh=SimpleFS_createFile(dh,name);
  SimpleFS_close(fh);
  readDir_test(dh,12);
  printf("SimpleFS_changeDir sotto ciao\n");
  ret=SimpleFS_changeDir(dh, "sotto ciao");
  sprintf(name, "%d",10);
  printf("SimpleFS_createFile 10\n");
  fh=SimpleFS_createFile(dh,name);
  SimpleFS_close(fh);
  if(ret!=FAILED) readDir_test(dh,13);
  printf("SimpleFS_changeDir ..\n");
ret=SimpleFS_changeDir(dh, "..");
  if(ret==FAILED) printf("Errore in changeDir ..\n");
  if(ret!=FAILED) readDir_test(dh,14);
  printf("SimpleFS_changeDir ..\n");
  ret=SimpleFS_changeDir(dh, "..");
  if(ret==FAILED) printf("Errore in changeDir ..\n");
  if(ret!=FAILED) readDir_test(dh,15);

  bitmap_info(dh->sfs->disk);

  printf("SimpleFS_remove ciao\n");
  ret=SimpleFS_remove(dh, "ciao");
  if(ret==FAILED) printf("Errore in remove ciao\n");
  if(ret!=FAILED) readDir_test(dh,16);

  bitmap_info(dh->sfs->disk);
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
  res=DiskDriver_load(fs->disk,fs->filename);
  CHECK_ERR(res==FAILED,"can't load the fs");
  DirectoryHandle *dh=SimpleFS_init(fs,disk);
  //createFile_openFile_closeFile_test(dh);
	read_seek_write_test(dh);
  //readDir_changeDir_mkDir_remove_test(dh);


  DiskDriver_shutdown(disk);
  free(disk);
  free(dh->dcb);
	free(dh->current_block);
  free(dh);
  free(fs);
}
