#include "simplefs.h"

void SimpleFS_format(SimpleFS* fs){
    // disk initializing in which we clean the bitmap
    DiskDriver_init(fs->disk,fs->filename,fs->block_num);

    // getting the root block
    int rootblock=DiskDriver_getFreeBlock(fs->disk,0);
    CHECK_ERR(rootblock==FAILED,"can't get the root block from the created disk");

		// allocating the directory block
		FirstDirectoryBlock *root=(FirstDirectoryBlock*)malloc(sizeof(FirstDirectoryBlock));
		//we set all to 0 to avoid clutter when we write the directory on disk
		memset(root,0,sizeof(FirstDirectoryBlock));

    // creating the block header
    root->header.previous_block=MISSING;
    root->header.next_block=MISSING;
    root->header.block_in_file=CONTROL_BLOCK;

    // creating the directory CB
    root->fcb.directory_block=MISSING;
		root->fcb.block_in_disk=rootblock;
		strncpy(root->fcb.name,"/",2);
		root->fcb.size_in_blocks=1;
		root->fcb.size_in_bytes=sizeof(FirstDirectoryBlock);
		root->fcb.is_dir=DIRECTORY;

		//initializing the entries of the directory
		root->num_entries=0;
    int i=0;
    for(i=0;i<(int)((BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int));i++){
      root->file_blocks[i]=MISSING;
    }

    int res;
    //writing the directory on disk
    res=DiskDriver_writeBlock(fs->disk,(void*) root,rootblock);
		free(root);
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
  if(res==FAILED){
		free(block);
		return NULL;
	}

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

FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename){

  //firstly we check if we have at least three free block in the disk
  //one for the FirstFileBlock
  //one for the data (that's the minimum space required for a file)
  //one for an eventually needed new DirectoryBlock to register the file into the directory
  if(d->sfs->disk->header->free_blocks<3){
    return NULL;
  }

  //now we check if the file is already present in the given directory
  int i,res;
  //this variable is used to stop the search if we have already found a matching filename
  int searching=1;


  //we know how many entries has a directorybut we don't know how many entries per block we have se we calculate it
  int FDB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
  int DB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);

  // in here we will save the read FirstFileBlock to check if has a matching file name
  FirstFileBlock* file=(FirstFileBlock*) malloc(sizeof(FirstFileBlock));
  //we save the number of entries in the directory
  int dir_entries=d->dcb->num_entries;
  printf("dir_entries %d\n",dir_entries);

  // we search for all the files in the current directory block
  for(i=0;i<dir_entries && i<FDB_max_elements && searching;i++){
    //we get the file
    printf("file_blocks[%d]: %d\n",i,d->dcb->file_blocks[i]);
    res=DiskDriver_readBlock(d->sfs->disk,file,d->dcb->file_blocks[i]);
    CHECK_ERR(res==FAILED,"the directory contains a free block in the entry list");
    //we check if the file has the same name of the file we want to create
    searching=strncmp(file->fcb.name,filename,strlen(filename));
  }
  if(searching==0){
    free(file);
    return NULL;
  }
  //now we search in the other directory blocks which compose the directory list

  DirectoryBlock *dir=(DirectoryBlock*)malloc(sizeof(DirectoryBlock));
  int next_block=d->dcb->header.next_block;
  //we save the block in disk of the current directory block for future use
  int dir_block_in_disk=d->dcb->fcb.block_in_disk;
  //we adjust the number of remaining entries
  dir_entries-=FDB_max_elements;

  //we start the search in the rest of the chained list
  while(next_block!=MISSING){

    //we get the next directory block;
    dir_block_in_disk=next_block;
		// we clean our directory before getting the new one
		memset(dir,0,sizeof(DirectoryBlock));
    res=DiskDriver_readBlock(d->sfs->disk,dir,next_block);

		if(res==FAILED){
			free(file);
			free(dir);
			CHECK_ERR(res==FAILED,"the directory contains a free block in the entry chain");
		}

    //we search it
    for(i=0;i<dir_entries && i<DB_max_elements && searching;i++){
      //we get the file
      res=DiskDriver_readBlock(d->sfs->disk,file,dir->file_blocks[i]);
			if(res==FAILED){
				free(file);
				free(dir);
				CHECK_ERR(res==FAILED,"the directory contains a free block in the entry list");
			}

      //we check if the file has the same name of the file we want to create
      searching=strncmp(file->fcb.name,filename,strlen(filename));
    }
    if(searching==0){
			free(dir);
      free(file);
      return NULL;
    }
    //now we need to change block and to adjust the number of remaining entries to check
    dir_entries-=DB_max_elements;
    next_block=dir->header.next_block;
  }

  //we get a free block from the disk
  int file_block=DiskDriver_getFreeBlock(d->sfs->disk,0);
  CHECK_ERR(file_block==FAILED,"full disk on file creation");
	// we clear our variable before initializing it
	memset(file,0,sizeof(FirstFileBlock));
  //if we get to this point we can create the file
  file->header.block_in_file=0;
  file->header.next_block=MISSING;
  file->header.previous_block=MISSING;
  file->next_IndexBlock=MISSING;
  file->num_entries=0;
  file->fcb.is_dir=FILE;
  file->fcb.block_in_disk=file_block;
  file->fcb.directory_block=d->dcb->fcb.block_in_disk;
  strncpy(file->fcb.name,filename,strlen(filename));
  //i'm probably committing some errors in here
  // TODO CHECK!
  file->fcb.size_in_blocks=1;
  file->fcb.size_in_bytes=512;

  //we create the file on disk
  res=DiskDriver_writeBlock(d->sfs->disk,file,file_block);
  CHECK_ERR(res==FAILED,"can't write on the given free block");

  //we update the metadata into the directory

  //we get the index of the block in which we must update the data
  int dir_block;
  int dir_block_displacement;

  //we check if we must update only the FirstDirectoryBlock or other blocks
  if(d->dcb->num_entries+1<FDB_max_elements){
    //in this case we have allocated only the FirstDirectoryBlock
    dir_block=0;
    dir_block_displacement=(d->dcb->num_entries)%FDB_max_elements;
  }else{
    dir_block=(d->dcb->num_entries-FDB_max_elements+1)/DB_max_elements;
    dir_block_displacement=(d->dcb->num_entries-FDB_max_elements+1)%DB_max_elements;
  }
  //if the the DirectoryBlock with index 1 exists we can use the last block scanned while we were searching
  //for a file with the same filename to update the directory info or create a new block linked to that one
  if(d->dcb->header.next_block!=MISSING){
    //in this case we have at least a DirectoryBlock
    if(dir->header.block_in_file==dir_block){
			if(dir->file_blocks[dir_block_displacement]!=MISSING){
				free(file);
				free(dir);
				CHECK_ERR(dir->file_blocks[dir_block_displacement]!=MISSING,"occupied dir entry");
			}
      dir->file_blocks[dir_block_displacement]=file_block;
    }else{
      //if we get here we must allocate a new directory block
      int new_dir_block=DiskDriver_getFreeBlock(d->sfs->disk,0);
      CHECK_ERR(new_dir_block,"full disk on directory update");

      DirectoryBlock *new_dir=(DirectoryBlock*)malloc(sizeof(DirectoryBlock));
      new_dir->header.block_in_file=dir->header.block_in_file+1;
      new_dir->header.next_block=MISSING;
      new_dir->header.previous_block=dir_block_in_disk;
      new_dir->file_blocks[0]=file_block;
      dir->header.next_block=new_dir_block;
      //we write the new dir_block
      res=DiskDriver_writeBlock(d->sfs->disk,&new_dir,new_dir_block);
			free(new_dir);
			if(res==FAILED){
				free(file);
				free(dir);
				CHECK_ERR(res==FAILED,"can't write the new dir_block on disk");
			}

    }
    //we write the updated dir_block
    res=DiskDriver_writeBlock(d->sfs->disk,dir,dir_block_in_disk);
		if(res==FAILED){
			free(file);
			CHECK_ERR(res==FAILED,"can't write the updated dir_block on disk");
		}

  }else{
    // in this case we must update only the FirstDirectoryBlock and on the worst case create a new block

    //we update only the FirstDirectoryBlock
    if(dir_block==0){
			if(d->dcb->file_blocks[dir_block_displacement]!=MISSING){
				free(file);
				CHECK_ERR(d->dcb->file_blocks[dir_block_displacement]!=MISSING,"occupied dir entry");
			}

      d->dcb->file_blocks[dir_block_displacement]=file_block;
    }else{
      //if we get here we must allocate a new directory block
      int new_dir_block=DiskDriver_getFreeBlock(d->sfs->disk,0);
			if(new_dir_block==FAILED){
				free(file);
				CHECK_ERR(new_dir_block==FAILED,"full disk on directory update");
			}


      DirectoryBlock *new_dir=(DirectoryBlock*)malloc(sizeof(DirectoryBlock));
      new_dir->header.block_in_file=d->dcb->header.block_in_file+1;
      new_dir->header.next_block=MISSING;
      new_dir->header.previous_block=d->dcb->fcb.block_in_disk;
      new_dir->file_blocks[0]=file_block;
      d->dcb->header.next_block=new_dir_block;
      //we write the new dir_block
      res=DiskDriver_writeBlock(d->sfs->disk,new_dir,new_dir_block);
			free(new_dir);
			if(res==FAILED){
				free(file);
			}
      CHECK_ERR(res==FAILED,"can't write the new dir_block on disk");
    }
  }
  //now we can safely deallocate dir
  free(dir);
  printf("updating dcb->num_entries\n");
  d->dcb->num_entries+=1;
  //we write the updated FirstDirectoryBlock
  res=DiskDriver_writeBlock(d->sfs->disk,d->dcb,d->dcb->fcb.block_in_disk);
	if(res==FAILED){
		free(file);
	}
  CHECK_ERR(res==FAILED,"can't write the updated FirstDirectoryBlock on disk");
  printf("wrote dcb changes\n");
  //after creating the file we create a file handle
	FileHandle *fh=(FileHandle*)malloc(sizeof(FileHandle));
  fh->current_block=&(file->header);
  fh->directory=d->dcb;
  fh->fcb=file;
  fh->pos_in_file=0;
  fh->sfs=d->sfs;
  return fh;
}
