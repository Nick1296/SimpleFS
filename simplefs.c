#include "simplefs.h"
#include "simplefs_aux.h"

void SimpleFS_format(SimpleFS* fs){
	//we check that we have been given valid data
	if(fs==NULL){
		return;
	}
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
		root->header.block_in_disk=rootblock;
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
	//we check that we have been given valid data
	if(fs==NULL || disk==NULL){
		return NULL;
	}
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
  handle->current_block=(BlockHeader*)malloc(sizeof(BlockHeader));
  memcpy(handle->current_block,&(block->header),sizeof(BlockHeader));
  handle->dcb=block;
  handle->directory=NULL;
  handle->pos_in_dir=0;
  handle->pos_in_block=0;

  return handle;
}

FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename){
	//we check that we have been given valid data
	if(d==NULL || strlen(filename)==0){
		return NULL;
	}

  //firstly we check if we have at least two free block in the disk
  //one for the FirstFileBlock
  //one for the data (that's the minimum space required for a file)
  //one for an eventually needed new DirectoryBlock to register the file into the directory
  if(d->sfs->disk->header->free_blocks<2){
    return NULL;
  }
  int res;

	SearchResult *search=SimpleFS_search(d,filename);
	//if the file already exists we return NULL
	if(search->result==SUCCESS){
		free(search->element);
		if(search->last_visited_dir_block!=NULL){
			free(search->last_visited_dir_block);
		}
		free(search);
		return NULL;
	}
	//we can deallocate the SearchResult because it isn't needed anymore
	free(search);
  FirstFileBlock* file=(FirstFileBlock*) malloc(sizeof(FirstFileBlock));
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
  file->header.block_in_disk=file_block;
  file->fcb.directory_block=d->dcb->header.block_in_disk;
  strncpy(file->fcb.name,filename,strlen(filename));
  file->fcb.size_in_blocks=0;
  file->fcb.size_in_bytes=0;

	//initializing the entries of the index cointained in the fcb
	file->num_entries=0;
	memset(file->blocks,MISSING,sizeof(int)*((BLOCK_SIZE-sizeof(FileControlBlock) - sizeof(BlockHeader)-sizeof(int)-sizeof(int))/sizeof(int)));

  //we create the file on disk
  res=DiskDriver_writeBlock(d->sfs->disk,file,file_block);
  CHECK_ERR(res==FAILED,"can't write on the given free block");

  //we update the metadata into the directory
	Dir_addentry(d,file_block);

  //after creating the file we create a file handle
	FileHandle *fh=(FileHandle*)malloc(sizeof(FileHandle));
	//the current block in the creation is the header of the fcb
  fh->current_block=(BlockHeader*)malloc(sizeof(BlockHeader));
	memcpy(fh->current_block,&(file->header),sizeof(BlockHeader));
	//we allocate and initialize the current index block
	fh->current_index_block=(BlockHeader*)malloc(sizeof(BlockHeader));
	fh->current_index_block->block_in_disk=file->header.block_in_disk;
	//we have 0 beacuse our index is the fcb
	fh->current_index_block->block_in_file=0;
	fh->current_index_block->next_block=MISSING;
	fh->current_index_block->previous_block=MISSING;
	//we copy the FirstDirectoryBlock from the DirectoryHandle
  fh->directory=(FirstDirectoryBlock*)malloc(sizeof(FirstDirectoryBlock));
	memcpy(fh->directory,d->dcb,sizeof(FirstDirectoryBlock));
  fh->fcb=file;
  fh->pos_in_file=0;
  fh->sfs=d->sfs;
  return fh;
}

// reads in the (preallocated) blocks array, the name of all files in a directory
int SimpleFS_readDir(char** names, DirectoryHandle* d){

  char isDir[] = " ->is dir\n   ";
  char isFile[] = " ->is file\n   ";
  char newLine[] = "\n   ";
  char str[] = "directory empty\n";

  int FDB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
	int Dir_Block_max_elements=(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
  int dir_entries=d->dcb->num_entries;
  int i, res, pos=0;

  int dim_names = (dir_entries+1)*((25+FILENAME_MAX_LENGTH)*sizeof(char));
  *names = (char*)malloc(dim_names);
  memset(*names, 0, dim_names);

  strncpy(*names, d->dcb->fcb.name, strlen(d->dcb->fcb.name));
  strcat(*names, newLine);

  if(dim_names==1*((FILENAME_MAX_LENGTH+25)*sizeof(char))){
    strcat(*names, str);
    return SUCCESS;
  }
  FirstDirectoryBlock* file=(FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
	DirectoryBlock* db = (DirectoryBlock*)malloc(sizeof(DirectoryBlock));

  // We search for all the files in the current directory block
  for(i=0;i<dir_entries;i++){
    // We get the file
    memset(file, 0, sizeof(FirstDirectoryBlock));
		if(i < FDB_max_elements){
			res=DiskDriver_readBlock(d->sfs->disk, file, d->dcb->file_blocks[i]);
			CHECK_ERR(res==FAILED, "Error read dir block in simplefs_readDir");
		}
		else{
			// File contenuto nei DB della directory genitore
			int logblock=((i-FDB_max_elements)/Dir_Block_max_elements)+1;
			int j, block = d->dcb->header.next_block;
			if(pos!=logblock) memset(db, 0, sizeof(DirectoryBlock));
			for(j=0; j<logblock && pos!=logblock; j++){
				res=DiskDriver_readBlock(d->sfs->disk , db, block);
				CHECK_ERR(res==FAILED, "Error on read of DirectoryBlock in simpleFS_readDir");
				block = db->header.next_block;
				pos=j+1;
			}
			res=DiskDriver_readBlock(d->sfs->disk, file, db->file_blocks[(i-FDB_max_elements)%Dir_Block_max_elements]);
			CHECK_ERR(res==FAILED, "Error read dir block in simplefs_readDir");
		}

    // Copy data of files of current directory
    sprintf((*names)+strlen(*names), "%d", file->fcb.size_in_bytes);
    sprintf((*names)+strlen(*names), "%c", ' ');
		strncat((*names)+strlen(*names), file->fcb.name, strlen(file->fcb.name));
		int len=strlen(*names);
    if(file->fcb.is_dir==DIRECTORY) strncat((*names)+len, isDir, strlen(isDir));
    else strncat((*names)+len, isFile, strlen(isFile));
  }

  memset(*names+strlen(*names)-1, 0,1);
  free(file);
	free(db);
  return SUCCESS;
}

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
int SimpleFS_changeDir(DirectoryHandle* d, const char* dirname){
  // Check if passed parameters are valid
  if(d == NULL || strlen(dirname)==0) return FAILED;

  int res;

  // Check if the directory where want to move is the current workdir
  if(strncmp(".", dirname, strlen(dirname))==0) return SUCCESS;

  // Check if the directory where want to move is the parent directory
  if(strncmp("..", dirname, strlen(dirname))==0){
    // Check if workdir is root dir
    if(d->directory==NULL) return SUCCESS;

    // We populate the handle according to the content informations
    DirectoryHandle* handle=(DirectoryHandle*) malloc(sizeof(DirectoryHandle));
		DirectoryHandle* father=(DirectoryHandle*) malloc(sizeof(DirectoryHandle));
    handle->sfs=d->sfs;
    handle->dcb=d->directory;

		memset(father, 0, sizeof(DirectoryHandle));
    res=DiskDriver_readBlock(d->sfs->disk, father, d->directory->header.block_in_disk);
    CHECK_ERR(res==FAILED,"Error read parent directory");

    handle->pos_in_dir=father->pos_in_dir;
    handle->pos_in_block=father->pos_in_block;

    // Check if directory have parent directory
    if(d->directory->fcb.directory_block!=MISSING){
      handle->directory = malloc(sizeof(FirstDirectoryBlock));
      memset(handle->directory, 0, sizeof(FirstDirectoryBlock));
      res=DiskDriver_readBlock(d->sfs->disk, handle->directory, d->directory->fcb.directory_block);
      CHECK_ERR(res==FAILED,"Error read parent directory");
    } else handle->directory = NULL;

    free(d->dcb);

    // Copy the calculated information
    d->sfs = handle->sfs;
    d->dcb=handle->dcb;
		memcpy(d->current_block,&(d->dcb->header),sizeof(BlockHeader));
    d->pos_in_dir=handle->pos_in_dir;
    d->pos_in_block=handle->pos_in_block;
    d->directory=handle->directory;

    free(handle);

    return SUCCESS;
  }

	SearchResult *search=SimpleFS_search(d, dirname);
	//if the dir already exists we return FAILED
	if(search->result==SUCCESS){
    FirstDirectoryBlock* file = ((FirstDirectoryBlock*)search->element);

    // we populate the handle according to the readed block
    DirectoryHandle* handle=(DirectoryHandle*) malloc(sizeof(DirectoryHandle));
    memset(handle, 0, sizeof(DirectoryHandle));
    handle->sfs=d->sfs;
    handle->dcb=file;
    handle->pos_in_dir=0;
    handle->pos_in_block=0;

    // Check if directory have parent directory
    if(file->fcb.directory_block!=MISSING){
      handle->directory = malloc(sizeof(FirstDirectoryBlock));
      memset(handle->directory, 0, sizeof(FirstDirectoryBlock));
      res=DiskDriver_readBlock(d->sfs->disk, handle->directory, file->fcb.directory_block);
      CHECK_ERR(res==FAILED,"Error read parent directory");
    }
    else handle->directory = NULL;

    free(d->dcb);
		if(d->directory!=NULL){
			free(d->directory);
		}

    // Copy the calculated information
    d->sfs = handle->sfs;
		memcpy(d->current_block,&(file->header),sizeof(BlockHeader));
    d->dcb=handle->dcb;
    d->pos_in_dir=handle->pos_in_dir;
    d->pos_in_block=handle->pos_in_block;
    d->directory=handle->directory;

    free(handle);
    if(search->last_visited_dir_block!=NULL) free(search->last_visited_dir_block);
    free(search);

    return SUCCESS;
  }else free(search);

  return FAILED;
 }

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success -1 on error
int SimpleFS_mkDir(DirectoryHandle* d, const char* dirname){
   // Check if passed parameters are valid
  if(d == NULL || strlen(dirname)==0) return FAILED;

	SearchResult *search=SimpleFS_search(d, dirname);
	//if the dir already exists we return FAILED
	if(search->result==SUCCESS){
		free(search->element);
		if(search->last_visited_dir_block!=NULL){
			free(search->last_visited_dir_block);
		}
    free(search);
		return FAILED;
	}
  free(search);

  // getting the root block
  int rootblock=DiskDriver_getFreeBlock(d->sfs->disk,0);
  if(rootblock==FAILED) return FAILED;

  // allocating the directory block
  FirstDirectoryBlock *root=(FirstDirectoryBlock*)malloc(sizeof(FirstDirectoryBlock));
  //we set all to 0 to avoid clutter when we write the directory on disk
  memset(root,0,sizeof(FirstDirectoryBlock));

  // creating the block header
  root->header.previous_block=MISSING;
  root->header.next_block=MISSING;
  root->header.block_in_file=CONTROL_BLOCK;

  // creating the directory CB
  root->fcb.directory_block=d->dcb->header.block_in_disk;
  root->header.block_in_disk=rootblock;
  strncpy(root->fcb.name, dirname, strlen(dirname));
  root->fcb.size_in_blocks=0;
  root->fcb.size_in_bytes=0;
  root->fcb.is_dir=DIRECTORY;

  //initializing the entries of the directory
  root->num_entries=0;
  memset(root->file_blocks,MISSING,((BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int))*sizeof(int));

  //we update the parent directory
  Dir_addentry(d,root->header.block_in_disk);
  int res;
  //writing the directory on disk
  res=DiskDriver_writeBlock(d->sfs->disk,(void*) root,rootblock);
  free(root);
  if(res==FAILED) return FAILED;

  res=DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->header.block_in_disk);
  if(res==FAILED) return FAILED;

  return SUCCESS;
}

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int SimpleFS_remove(DirectoryHandle* d, const char* filename){
   // Check if passed parameters are valid
  if(d == NULL || strlen(filename)==0) return FAILED;

  int FDB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
	int Dir_Block_max_elements=(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
  int dir_entries=d->dcb->num_entries;
  int i, res;

  if(dir_entries==0) return SUCCESS;

	// Chiamare search
	SearchResult* sRes = SimpleFS_search(d, filename);
	if(sRes->result==FAILED){
		free(sRes);
		return FAILED;
	}

	// Check if the target file is a directory
	if(sRes->type==DIRECTORY){
  	FirstDirectoryBlock* file=(FirstDirectoryBlock*)sRes->element;

		// We populate the handle according to the content informations
		DirectoryHandle* handle=(DirectoryHandle*) malloc(sizeof(DirectoryHandle));
		memset(handle, 0, sizeof(DirectoryHandle));
		handle->sfs=d->sfs;
		handle->current_block=&(file->header);
		handle->dcb=file;
		handle->directory = d->dcb;
		handle->pos_in_dir=0;
		handle->pos_in_block=0;

		// search eventuall child dir
		res=SimpleFS_remove_rec(handle);
		if(res==FAILED){
			free(file);
			free(handle);
			free(sRes);
			return FAILED;
		}

		// Remove the files contained in the directory
		DirectoryBlock* db = (DirectoryBlock*)malloc(sizeof(DirectoryBlock));
		FirstFileBlock* fh = (FirstFileBlock*)malloc(sizeof(FirstFileBlock));
		int z, pos=0;

		for(z=0; z<handle->dcb->num_entries; z++){
			if(z < FDB_max_elements){
				res=DiskDriver_readBlock(handle->sfs->disk, fh, handle->dcb->file_blocks[z]);
				CHECK_ERR(res==FAILED, "Error read dir block in simplefs_remove");
				if(fh->fcb.is_dir==FILE) SimpleFS_removeFileOnDir(handle, (void*)fh, z);
			}
			else{
				// File contenuto nei DB della directory genitore
				int logblock=((z-FDB_max_elements)/Dir_Block_max_elements)+1;
				int block = handle->dcb->header.next_block;
				for(i=0; i<logblock && pos!=logblock; i++){
					res=DiskDriver_readBlock(handle->sfs->disk , db, block);
					CHECK_ERR(res==FAILED, "Error on read of DirectoryBlock in simpleFS_remove");
					block = db->header.next_block;
					pos=i+1;
				}
				res=DiskDriver_readBlock(handle->sfs->disk, fh, db->file_blocks[(z-FDB_max_elements)%Dir_Block_max_elements]);
				CHECK_ERR(res==FAILED, "Error read dir block in simplefs_remove");
				if(fh->fcb.is_dir==FILE) SimpleFS_removeFileOnDir(handle, (void*)fh, z);
			}
		}
		free(fh);
		free(db);

		// Remove that the directory in the parent directory
		res=SimpleFS_removeChildDir(handle);
		CHECK_ERR(res==FAILED, "Error in remove a child dir");
		free(handle);
		free(file);
		free(sRes);

		// We write the updated FirstDirectoryBlock
		res=DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->header.block_in_disk);
		if(res==FAILED) return FAILED;

		return SUCCESS;
	}else{
		// Elimino il file dalla directory in cui risiede
		SimpleFS_removeFileOnDir(d, sRes->element, sRes->pos_in_block);
		free(sRes);

		return SUCCESS;
	}

	free(sRes);
  return FAILED;
}

FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename){
	//we check that we have been given valid data
	if(d==NULL || strlen(filename)==0){
		return NULL;
	}
	//firstly we need to search the file we want to open
	SearchResult *search=SimpleFS_search(d,filename);
	if(search->result==FAILED){
		if(search->last_visited_dir_block!=NULL){
			free(search->last_visited_dir_block);
		}
		free(search);
		return NULL;
	}
	//we create our handle
	FileHandle *file=(FileHandle*)malloc(sizeof(FileHandle));
	file->fcb=(FirstFileBlock*)search->element;
	file->pos_in_file=0;
	file->sfs=d->sfs;
	file->directory=(FirstDirectoryBlock*)malloc(sizeof(FirstDirectoryBlock));
	memcpy(file->directory,d->dcb,sizeof(FirstDirectoryBlock));
	//the current block in the creation is the header of the fcb
	file->current_block=(BlockHeader*)malloc(sizeof(BlockHeader));
	memcpy(file->current_block,&(((FirstFileBlock*)search->element)->header),sizeof(BlockHeader));
	//we allocate and initialize the current index block
	file->current_index_block=(BlockHeader*)malloc(sizeof(BlockHeader));
	file->current_index_block->block_in_disk=file->fcb->header.block_in_disk;
	//we have 0 beacuse our index is the fcb
	file->current_index_block->block_in_file=0;
	file->current_index_block->next_block=file->fcb->next_IndexBlock;
	file->current_index_block->previous_block=MISSING;
	// we cleanup the result of our search
	if(search->last_visited_dir_block!=NULL){
		free(search->last_visited_dir_block);
	}
	free(search);
	return file;
}

void SimpleFS_close(FileHandle* f){
	//we check that we have been given valid data
	if(f==NULL){
		return;
	}

	//we deallocate BlockHeader which indicates the current data block in the file
	free(f->current_block);
	//we deallocate the current BlockHeader of the index
	free(f->current_index_block);
	//we need to deallocate the FirstFileBlock
	free(f->fcb);
	//we deallocate the directory block in which the file is contained
	free(f->directory);
	//now we can deallocate the file handle
	free(f);
}

// returns pos on success -1 on error (file too short)
int SimpleFS_seek(FileHandle* f, int pos){
	//we check if the position is within the file dimension
	if(f==NULL || pos<0 ||pos>f->fcb->fcb.size_in_bytes){
		return FAILED;
	}
	//we calculate the block in which we need to move
	int DB_max_elements=BLOCK_SIZE-sizeof(BlockHeader);
	int block_in_file=(pos+DB_max_elements-1)/DB_max_elements;
	int res;
	//we check if the block we need exists by checking on the index
	int pos_in_disk=SimpleFS_getIndex(f,block_in_file);
	if(pos_in_disk==FAILED){
		//the block doesn't exists
		return FAILED;
	}else{
		//we update the cursor position in the file
		f->pos_in_file=pos;
		//we read the block to get the BlockHeader
		FileBlock* block=(FileBlock*)malloc(sizeof(FileBlock));
		res=DiskDriver_readBlock(f->sfs->disk,block,pos_in_disk);
		if(res==FAILED){
			free(block);
			CHECK_ERR(res==FAILED,"can't read block while seeking");
		}
		//we copy the BlockHeader in the current_block
		memcpy(f->current_block,&(block->header),sizeof(BlockHeader));
		free(block);
		return pos;
	}
}

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
//the cursor at the end of the write points to the last byte written
int SimpleFS_write(FileHandle* f, void* data, int size){
	//we check that we have been given valid data
	if(f==NULL || data==NULL || size<=0){
		return 0;
	}
	//we calculate how many byte can fill a data block
	int DB_max_elements=BLOCK_SIZE-sizeof(BlockHeader);
	int bytes_written=0;
	//we calculate how many blocks we need to write
	int blocks_needed=(size+DB_max_elements-1)/DB_max_elements;

	/* current position is stored in FileHandle as pos_in_file,
	* but we need to express it using current_block
	* (which is alredy updated according to pos_in_file) and displacement
	*/
	int displacement;
	//that's the result of our last write
	int res=SUCCESS;

	// this is the last block written or the fcb if we write in a blank file,
	void* last_block=malloc(BLOCK_SIZE);
	FileBlock* block=(FileBlock*)malloc(sizeof(FileBlock));

	//we must write on the current block first, but it could be the fcb
	int current_block_written=0;
	// we must check if there is a block after the current block of data to ber overwritten
	int overwrite=1;
	//we check if the current block is the fcb or if the last block is full
	if(f->current_block->block_in_file==0){
		current_block_written=1;
		overwrite=0;
		//we initialize the last block to be able to link the FFB to the next data block
		memcpy(last_block,f->fcb,BLOCK_SIZE);
	}else if(f->pos_in_file==f->fcb->fcb.size_in_blocks*DB_max_elements){
		overwrite=0;
		current_block_written=1;
		res=DiskDriver_readBlock(f->sfs->disk,last_block,f->current_block->block_in_disk);
		CHECK_ERR(res==FAILED,"can't read the current block");
	}

	//we set the current size of the file and the position in the file accordin to the current block
	f->fcb->fcb.size_in_blocks=(f->current_block->block_in_file>0)?f->current_block->block_in_file-1:0;
	f->fcb->fcb.size_in_bytes=f->pos_in_file;

	while(blocks_needed>0 && res!=FAILED){
		//we clear our block in memory to avoid writing clutter on disk
		memset(block,0,sizeof(FileBlock));
		if(current_block_written && !overwrite){
			//in this case we are writing at the beginning of the file
			//or need a new block to keep writing
			res=DiskDriver_getFreeBlock(f->sfs->disk,f->current_block->block_in_disk);
			if(res!=FAILED){
				//we initialize the new block
				block->header.block_in_disk=res;
				block->header.next_block=MISSING;
				block->header.previous_block=f->current_block->block_in_disk;
				block->header.block_in_file=f->current_block->block_in_file+1;
				//we update the last written block
				((FileBlock*)last_block)->header.next_block=block->header.block_in_disk;
				//if the block is the ffb we update our in memory copy
				if(f->current_block->block_in_file==0){
					f->fcb->header.next_block=block->header.block_in_disk;
				}
				//we write the changes on the disk
				res=DiskDriver_writeBlock(f->sfs->disk,last_block,((FileBlock*)last_block)->header.block_in_disk);
				//we update our current_block
				memcpy(f->current_block,&(block->header),sizeof(BlockHeader));

				//when allocating a new block our displacement is 0
				displacement=0;

			}
		}else{
			//if we get here we only need to load the block from disk
			if(current_block_written && overwrite){
				res=DiskDriver_readBlock(f->sfs->disk,block,f->current_block->next_block);
				//we update our current_block
				memcpy(f->current_block,&(block->header),sizeof(BlockHeader));
			}else{
				res=DiskDriver_readBlock(f->sfs->disk,block,f->current_block->block_in_disk);
			}
		}
		if(overwrite==0 && current_block_written==1){
			displacement=0;
		}else{
			displacement=(f->pos_in_file-DB_max_elements*(f->current_block->block_in_file-1)+bytes_written);
		}
		//now we need to write in the block
		if(res!=FAILED){
			if(size-bytes_written>DB_max_elements-displacement){
				//we fill the block if we need to write more than one block
				memcpy(block->data+displacement,data+bytes_written,DB_max_elements-displacement);
			}else{
				//or we write only one portion of the block
				memcpy(block->data+displacement,data+bytes_written,size-bytes_written);
			}
			//if we have finished writing and we are overwriting blocks we break the chain
			if(overwrite && blocks_needed-1==0){
				block->header.next_block=MISSING;
			}
			//we write the block on disk
			res=DiskDriver_writeBlock(f->sfs->disk,block,block->header.block_in_disk);
			//and if we allocated a new block we add it to the indexes
			if(current_block_written && !overwrite && res!=FAILED){
				res=SimpleFS_addIndex(f,block->header.block_in_disk);
			}
			//and we indicate that our current block has been written
			current_block_written=1;
			//check if we need to overwrite the next blocks or allcocate them
			if(f->current_block->next_block!=MISSING){
				overwrite=1;
			}else{
				overwrite=0;
			}
			//we update the cursor and the bytes written
			if(res!=FAILED){
				if(size-bytes_written>DB_max_elements-displacement){
					bytes_written+=DB_max_elements-displacement;
				}else{
					bytes_written+=size-bytes_written;
				}
				//we update the size in blocks of the file
				f->fcb->fcb.size_in_blocks++;
				blocks_needed--;
			}
		}
		//we copy our current block in the last block, so we can link the next block if needed
		memcpy(last_block,block,sizeof(FileBlock));
	}
	f->fcb->fcb.size_in_bytes+=bytes_written;
	f->pos_in_file+=bytes_written;

	//we remove the blocks after the actual block from the indexes and free them
	if(f->current_block->next_block!=MISSING){
		SimpleFS_clearIndexes(f,block->header.block_in_file+1);
		f->current_block->next_block=MISSING;
	}

	//we write the changes on our Fcb
	res=DiskDriver_writeBlock(f->sfs->disk,f->fcb,f->fcb->header.block_in_disk);
	CHECK_ERR(res==FAILED,"can't write the chenges of the fcb");
	free(block);
	free(last_block);
	return bytes_written;
}

// reads in the file, at current position size bytes and stores them in data
// returns the number of bytes read
int SimpleFS_read(FileHandle* f, void* data, int size){
	//we check that we have been given valid data
	if(f==NULL || data==NULL || size<=0){
		return 0;
	}
	int bytes_read=0;
	//we calcualte the bytes to read, which are the minimum between the size of the buffer and the size of the file
	int bytes_to_read=((size<f->fcb->fcb.size_in_bytes-f->pos_in_file)?size:f->fcb->fcb.size_in_bytes-f->pos_in_file);
	FileBlock *block=(FileBlock*)malloc(sizeof(FileBlock));
	//we calculate the maximum number of elements in athe block
	int DB_max_elements=BLOCK_SIZE-sizeof(BlockHeader);
	//we calculate thedisplacement in the first block;
	int displacement;
	int res=SUCCESS;
	//now we start reading
	while(bytes_read<bytes_to_read && res==SUCCESS){
		displacement=(f->pos_in_file-DB_max_elements*(f->current_block->block_in_file-1)+bytes_read);

		//we clean our block in memory
		memset(block,0,sizeof(FileBlock));
		// we read the current block
		res=DiskDriver_readBlock(f->sfs->disk,block,f->current_block->block_in_disk);
		//now we can read the data
		if(bytes_to_read-bytes_read>DB_max_elements-displacement && res==SUCCESS){
			//we read all the block
			memcpy(data+bytes_read,(block->data)+displacement,DB_max_elements-displacement);
			//we update the number of bytes read
			bytes_read+=DB_max_elements-displacement;
		}else{
			//we read only one portion of the block
			memcpy(data+bytes_read,block->data+displacement,bytes_to_read-bytes_read);
			//we update the number of bytes read
			bytes_read+=bytes_to_read-bytes_read;
		}

		if(bytes_read<bytes_to_read){
			//we move one block further
			if(f->current_block->next_block==MISSING){
				//if the next block is missing we can't read further
				free(block);
				f->pos_in_file+=bytes_read;
				return bytes_read;
			}
			res=DiskDriver_readBlock(f->sfs->disk,block,f->current_block->next_block);
			if(res!=FAILED){
				//we fetch the next_block
				memcpy(f->current_block,block,sizeof(BlockHeader));
			}
		}
	}

	//we update our position in the file
	f->pos_in_file+=bytes_read;
	free(block);
	return bytes_read;
}
