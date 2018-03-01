#include "simplefs.h"

void SimpleFS_format(SimpleFS* fs){
    // disk initializing in which we clean the bitmap
    DiskDriver_init(fs->disk,fs->filename,fs->num_blocks);
    
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
    control_block.name='/';
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