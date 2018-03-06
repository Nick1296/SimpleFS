#include "simplefs.h"

void SimpleFS_format(SimpleFS* fs){
    // disk initializing in which we clean the bitmap
    DiskDriver_init(fs->disk,fs->filename,fs->block_num);

    // getting the root block
    int rootblock=DiskDriver_getFreeBlock(fs->disk,0);
    CHECK_ERR(rootblock==FAILED,"can't get the root block from the created disk");
    // creating the block header
    BlockHeader header;
    header.previous_block=MISSING;
    header.next_block=MISSING;
    header.block_in_file=CONTROL_BLOCK;

    // creating the directory CB
    FileControlBlock control_block;
    control_block.directory_block=MISSING;
    control_block.block_in_disk=rootblock;
    strncpy(control_block.name,"/\0",2);
    control_block.size_in_blocks=1;
    control_block.size_in_bytes=sizeof(FirstDirectoryBlock);
    control_block.is_dir=DIRECTORY;

    // creating the directory block
    FirstDirectoryBlock root;
    root.header=header;
    root.fcb=control_block;
    root.num_entries=0;

    int res;
    //writing the directory on disk
    res=DiskDriver_writeBlock(fs->disk,(void*) &root,rootblock);
    CHECK_ERR(res==FAILED,"can't write to disk");
}

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk){
  fs->disk=disk;
  int res,current_block=disk->header->bitmap_blocks;
  FirstDirectoryBlock* block=(FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));

  //we read the first block after the block used by the bitmap, which is the directory block
  //remeber: we start from 0!
  res=DiskDriver_readBlock(fs->disk,block,current_block);
  if(res==FAILED) return NULL;

  // we populate the handle according to the readed block
  DirectoryHandle* handle=(DirectoryHandle*) malloc(sizeof(DirectoryHandle));
  handle->sfs=fs;
  handle->current_block=&(block->header);
  handle->dcb=block;
  handle->directory=NULL;
  handle->pos_in_dir=0;
  handle->pos_in_block=sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);

  return handle;

}
