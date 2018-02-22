#include "disk_driver.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define CHECK_ERR(cond,msg)  if (cond) {                       \
                              perror(msg);                     \
                              _exit(EXIT_FAILURE);             \
                             }

void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks){
  int res,exists=1,fd; //1= file already exists 0=file doesn't exists yet
  //checking if the file exists
  res=access(filename,F_OK);
  if(res==-1 && errno==ENOENT){
    exists=0;
  }else{
    CHECK_ERR(res==-1,"Can't test if the file exists");
    exists=1;
  }

  // opening the file and creating it if necessary
  fd=open(filename,O_CREAT | O_RDWR,0666);
  //checking if the open was successful
  CHECK_ERR(fd==-1,"error opening the file");

  //now we need to prepare the file to be mmapped because it needs to have the same dimension of the mmapped array
  //to do so we need to write something (/0) to the last byte of the file (num_blocks*BLOCK_SIZE)
  //we have repositioned the file pointer to the last byte
  res=lseek(fd,num_blocks*BLOCK_SIZE-1,SEEK_SET);
  CHECK_ERR(res==-1,"can't reposition pointer file");
  //now we write something so the file will actually have this dimension
  res=write(fd,"/0",1);
  CHECK_ERR(res==-1,"can't write into file");

  //mmap the file
  void* map=mmap(0,num_blocks*BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE,fd,0);
  CHECK_ERR(map==MAP_FAILED,"error mapping the file");

  //calculating the bitmap size (rounded up)
  int bitmap_size=(num_blocks+7)/8;
  //rounded up block occupation of DiskHeader and bitmap
  int occupation=(sizeof(DiskHeader)+BLOCK_SIZE-1)/BLOCK_SIZE-(bitmap_size*sizeof(char)+BLOCK_SIZE)/BLOCK_SIZE;
  //initializing the file if it doesn't exist
  if(!exists){
    //creating and initializing the disk header
    DiskHeader d;
    d.num_blocks=num_blocks;
    d.bitmap_blocks=bitmap_size;
    d.bitmap_entries=num_blocks-occupation;
    d.free_blocks=num_blocks-occupation;
    d.first_free_block=occupation+1;
    //writing the disk header to the mmapped file
    memcpy(map,&d,sizeof(DiskHeader));

    //creating the bitmap
    memset(map+sizeof(DiskHeader),0,bitmap_size*sizeof(char));
  }
  //populating the disk driver
  disk->header=map;
  disk->bitmap_data=map+sizeof(DiskHeader);
  disk->fd=fd;
}
