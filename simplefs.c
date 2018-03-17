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

// searches for a file or a directory given a DirectoryHandle and the element name
SearchResult* SimpleFS_search(DirectoryHandle* d, const char* name){

	// we  will save our results and all our intermediate variables in here
	SearchResult *result=(SearchResult*) malloc(sizeof(SearchResult));
	result->result=FAILED;
	result->dir_block_in_disk=MISSING;
	result->element=NULL;
	result->last_visited_dir_block=NULL;
	result->type=MISSING;

	//now we check if the file is already present in the given directory
	int i,res;
	//this variable is used to stop the search if we have already found a matching filename
	int searching=1;


	//we know how many entries has a directory but we don't know how many entries per block we have so we calculate it
	int FDB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
	int DB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);

	// in here we will save the read FirstFileBlock/FirstDirectoryBlock to check if has a matching file name
	void* element=malloc(BLOCK_SIZE);
	//we save the number of entries in the directory
	int dir_entries=d->dcb->num_entries;

	// we search for all the files in the current directory block
	for(i=0;i<dir_entries && i<FDB_max_elements && searching;i++){
		memset(element,0,BLOCK_SIZE);
		//we get the file
		res=DiskDriver_readBlock(d->sfs->disk,element,d->dcb->file_blocks[i]);
		if(res==FAILED){
			free(element);
			free(result);
			CHECK_ERR(res==FAILED,"the directory contains a free block in the entry list");
		}
		//we check if the file has the same name of the file we want to create
		searching=strncmp(((FileControlBlock*)(element+sizeof(BlockHeader)))->name,name,strlen(name));
	}
	if(searching==0){
		result->result=SUCCESS;
		result->dir_block_in_disk=MISSING;
		result->type=FILE;
		result->last_visited_dir_block=NULL;
		result->element=element;
		return result;
	}
	//now we search in the other directory blocks which compose the directory list

	int next_block=d->dcb->header.next_block;
	int dir_block_in_disk;
	//we save the block in disk of the current directory block for future use
	dir_block_in_disk=d->dcb->header.block_in_disk;
	//we adjust the number of remaining entries
	dir_entries-=FDB_max_elements;
	//we start the search in the rest of the chained list
	while(next_block!=MISSING){
		DirectoryBlock *dir=(DirectoryBlock*)malloc(sizeof(DirectoryBlock));

		//we get the next directory block;
		dir_block_in_disk=next_block;
		// we clean our directory before getting the new one
		memset(dir,0,sizeof(DirectoryBlock));
		res=DiskDriver_readBlock(d->sfs->disk,dir,next_block);

		if(res==FAILED){
			free(element);
			free(dir);
			free(result);
			CHECK_ERR(res==FAILED,"the directory contains a free block in the entry chain");
		}

		//we search it
		for(i=0;i<dir_entries && i<DB_max_elements && searching;i++){
			//we get the file
			res=DiskDriver_readBlock(d->sfs->disk,element,dir->file_blocks[i]);
			if(res==FAILED){
				free(element);
				free(dir);
				free(result);
				CHECK_ERR(res==FAILED,"the directory contains a free block in the entry list");
			}

			//we check if the file has the same name of the file we want to create
			searching=strncmp(((FileControlBlock*)(element+sizeof(BlockHeader)))->name,name,strlen(name));
		}
		if(searching==0){
			result->type=FILE;
			result->dir_block_in_disk=dir_block_in_disk;
			result->element=element;
			result->last_visited_dir_block=dir;
			result->result=SUCCESS;
		}
		//now we need to change block and to adjust the number of remaining entries to check
		dir_entries-=DB_max_elements;
		next_block=dir->header.next_block;
	}
	if(element!=NULL){
		free(element);
	}
	return result;
}

FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename){

  //firstly we check if we have at least two free block in the disk
  //one for the FirstFileBlock
  //one for the data (that's the minimum space required for a file)
  //one for an eventually needed new DirectoryBlock to register the file into the directory
  if(d->sfs->disk->header->free_blocks<2){
    return NULL;
  }
  int res;
	//we know how many entries has a directory but we don't know how many entries per block we have so we calculate it
	int FDB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
	int DB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);

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
	int i=0;
	for(i=0;i<(int)((BLOCK_SIZE-sizeof(FileControlBlock) - sizeof(BlockHeader)-sizeof(int)-sizeof(int))/sizeof(int));i++){
		file->blocks[i]=MISSING;
	}

  //we create the file on disk
  res=DiskDriver_writeBlock(d->sfs->disk,file,file_block);
  CHECK_ERR(res==FAILED,"can't write on the given free block");

  //we update the metadata into the directory

	//we use the current block, pos_in_block and pos_in_dir to determine the position where we
	//must write the new file in the directory
	//but it could be uninitialized!
	DirectoryBlock *dir=(DirectoryBlock*)malloc(sizeof(DirectoryBlock));
		while(d->current_block->next_block!=MISSING){
			DiskDriver_readBlock(d->sfs->disk,dir,d->current_block->next_block);
			CHECK_ERR(res==FAILED,"can't read the directory block");
			memcpy(d->current_block,&(dir->header),sizeof(BlockHeader));
		}
		d->pos_in_dir=d->dcb->num_entries;
		if(d->current_block->block_in_file==0){
			d->pos_in_block=d->pos_in_dir%FDB_max_elements-1;
		}else{
			d->pos_in_block=(d->pos_in_dir-FDB_max_elements-1)%DB_max_elements;
		}


  //we get the index of the block in which we must update the data
  int dir_block=d->current_block->block_in_file;
  int dir_block_displacement;
	if(d->dcb->num_entries==0){
		dir_block_displacement=0;
	}else{
		dir_block_displacement=d->pos_in_block+1;
	}


	if(dir_block==0){
		if(dir_block_displacement<FDB_max_elements){
			//in this case we can add the file directly in the FDB
			d->dcb->file_blocks[dir_block_displacement]=file_block;
		}else{
			//in this case we must allocate a directory block
			dir_block_displacement=0;

			int new_dir_block=DiskDriver_getFreeBlock(d->sfs->disk,0);
			if(new_dir_block==FAILED){
				free(file);
				CHECK_ERR(new_dir_block==FAILED,"full disk on directory update");
			}
			dir->header.block_in_file=d->dcb->header.block_in_file+1;
			dir->header.next_block=MISSING;
			dir->header.previous_block=d->dcb->header.block_in_disk;
			dir->file_blocks[dir_block_displacement]=file_block;
			d->dcb->header.next_block=new_dir_block;
			//we update the current_block
			memcpy(d->current_block,&(dir->header),sizeof(BlockHeader));
			res=DiskDriver_writeBlock(d->sfs->disk,dir,new_dir_block);
			CHECK_ERR(res==FAILED,"can't write the 2nd directory block");
		}
		free(dir);
	}else{
		// now we must check if in the last directory_block we have room for one more file
		if(dir_block_displacement<DB_max_elements){
			//in this case we can add the file directly in the DB
			dir->file_blocks[dir_block_displacement]=file_block;
			CHECK_ERR(res==-1,"can't write the 2nd directory block");
		}else{
			int new_dir_block=DiskDriver_getFreeBlock(d->sfs->disk,0);
			CHECK_ERR(new_dir_block,"full disk on directory update");
			dir_block_displacement=0;
			DirectoryBlock *new_dir=(DirectoryBlock*)malloc(sizeof(DirectoryBlock));
			new_dir->header.block_in_file=dir->header.block_in_file+1;
			new_dir->header.next_block=MISSING;
			new_dir->header.previous_block=search->dir_block_in_disk;
			new_dir->file_blocks[dir_block_displacement]=file_block;
			dir->header.next_block=new_dir_block;
			//we update the current_block
			memcpy(d->current_block,&(new_dir->header),sizeof(BlockHeader));
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
		res=DiskDriver_writeBlock(d->sfs->disk,dir,search->dir_block_in_disk);
		free(dir);
		if(res==FAILED){
			free(file);
			CHECK_ERR(res==FAILED,"can't write the updated dir_block on disk");
		}
	}
	//we update the metedata into the FDB
  d->dcb->num_entries++;

	//we update the metadata into the DirectoryHandle
	d->pos_in_dir++;
	d->pos_in_block=dir_block_displacement;
  //we write the updated FirstDirectoryBlock
  res=DiskDriver_writeBlock(d->sfs->disk,d->dcb,d->dcb->header.block_in_disk);
	if(res==FAILED){
		free(file);
		CHECK_ERR(res==FAILED,"can't write the updated FirstDirectoryBlock on disk");
	}
  //after creating the file we create a file handle
	FileHandle *fh=(FileHandle*)malloc(sizeof(FileHandle));
	//the current block in the creation is the header of the fcb
  fh->current_block=(BlockHeader*)malloc(sizeof(BlockHeader));
	memcpy(fh->current_block,&(file->header),sizeof(BlockHeader));
	//we allocate and initialize the current index block
	fh->current_index_block=(BlockHeader*)malloc(sizeof(BlockHeader));
	fh->current_index_block->block_in_disk=MISSING;
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
  int dir_entries=d->dcb->num_entries;
  int searching=1, i, res;

  int dim_names = (dir_entries+1)*(FILENAME_MAX_LENGTH*sizeof(char));
  *names = malloc(dim_names);
  memset(*names, 0, dim_names);

  strncpy(*names, d->dcb->fcb.name, strlen(d->dcb->fcb.name));
  strcat(*names, newLine);

  if(dim_names==1*FILENAME_MAX_LENGTH*sizeof(char)){
    strcat(*names, str);
    return SUCCESS;
  }
  FirstFileBlock* file=(FirstFileBlock*) malloc(sizeof(FirstFileBlock));

  // We search for all the files in the current directory block
  for(i=0;i<dir_entries && i<FDB_max_elements && searching;i++){
    // We get the file
    memset(file, 0, sizeof(FirstFileBlock));
    res=DiskDriver_readBlock(d->sfs->disk, file, d->dcb->file_blocks[i]);
    CHECK_ERR(res==FAILED,"the directory contains a free block in the entry list");

    // Copy name of files of current directory
    strncpy((*names)+strlen(*names), file->fcb.name, strlen(file->fcb.name));
    if(file->fcb.is_dir==DIRECTORY) strncpy((*names)+strlen(*names), isDir, strlen(isDir));
    else strncpy((*names)+strlen(*names), isFile, strlen(isFile));
  }

  memset(*names+strlen(*names)-1, 0,1);
  free(file);

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
    handle->sfs=d->sfs;
    handle->dcb=d->directory;
    handle->pos_in_dir=0;
    handle->pos_in_block=sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);

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
    FirstDirectoryBlock* file = (FirstDirectoryBlock*)search->element;

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
    free(search->last_visited_dir_block);
    free(search);

    return SUCCESS;
  }

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
  root->fcb.size_in_blocks=1;
  root->fcb.size_in_bytes=sizeof(FirstDirectoryBlock);
  root->fcb.is_dir=DIRECTORY;

  //initializing the entries of the directory
  root->num_entries=0;
  int i=0;
  for(i=0;i<(int)((BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int));i++){
    root->file_blocks[i]=MISSING;
  }

  //we write the updates on FirstDirectoryBlock of parent
  d->dcb->num_entries+=1;
  d->dcb->file_blocks[d->dcb->num_entries-1] = rootblock;

  int res;
  //writing the directory on disk
  res=DiskDriver_writeBlock(d->sfs->disk,(void*) root,rootblock);
  free(root);
  if(res==FAILED) return FAILED;

  res=DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->header.block_in_disk);
  if(res==FAILED) return FAILED;

  return SUCCESS;
}

// Removes files and directories recursively
int SimpleFS_remove_rec(DirectoryHandle* d){
   // Check if passed parameters are valid
  if(d == NULL) return FAILED;

  int FDB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
  int dir_entries=d->dcb->num_entries;
  int searching=1, i, res;

  if(dir_entries==0) return SUCCESS;

  FirstDirectoryBlock* file=(FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));

  // we search for all the files in the current directory block
  for(i=0;i<dir_entries && i<FDB_max_elements && searching;i++){
    //we get the file
    memset(file, 0, sizeof(FirstDirectoryBlock));
    res=DiskDriver_readBlock(d->sfs->disk, file, d->dcb->file_blocks[i]);
    CHECK_ERR(res==FAILED,"the directory contains a free block in the entry list");

    // Check if the target file is a directory
    if(file->fcb.is_dir==DIRECTORY){
      // We populate the handle according to the content informations
      DirectoryHandle* handle=(DirectoryHandle*) malloc(sizeof(DirectoryHandle));
      memset(handle, 0, sizeof(DirectoryHandle));
      handle->sfs=d->sfs;
      handle->current_block=&(file->header);
      handle->dcb=file;
      handle->directory = d->dcb;
      handle->pos_in_dir=0;
      handle->pos_in_block=sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);
      res=SimpleFS_remove_rec(handle);
      if(res==FAILED){
        free(file);
        free(handle);
        return FAILED;
      }
      free(handle);
    }
    // Remove the files contained in the directory
    int i;
    for(i=0; i<file->num_entries; i++){
      // TODO Free all block occupied that file
      DiskDriver_freeBlock(d->sfs->disk, file->file_blocks[i]);
      file->file_blocks[i]=MISSING;
    }
    file->num_entries-=i;

    // Remove that the directory in the parent directory
    int j=0;
    for(i=0; i<d->dcb->num_entries; i++){
      if(d->dcb->file_blocks[i]!=file->header.block_in_disk){
        d->dcb->file_blocks[j]=d->dcb->file_blocks[i];
        j++;
      }
      else{
        // Free all block occupied that directory
        // TODO CHECK IF IS CORRECT!
        if(file->header.block_in_file==CONTROL_BLOCK){
          int res, block = file->header.next_block;
          DirectoryBlock* app = malloc(sizeof(DirectoryBlock));
          while(block!=MISSING){
            memset(app, 0, sizeof(DirectoryBlock));
            res=DiskDriver_readBlock(d->sfs->disk, (void*)app, block);
            if(res==FAILED) return FAILED;
            res=DiskDriver_freeBlock(d->sfs->disk, block);
            if(res==FAILED) return FAILED;
            block = app->header.next_block;
          }
          free(app);
        }

        DiskDriver_freeBlock(d->sfs->disk ,d->dcb->file_blocks[i]);
        d->dcb->file_blocks[i]=MISSING;
      }
    }
    free(file);

    // Update num entry of directory
    d->dcb->num_entries=j;

    // We write the updated FirstDirectoryBlock
    res=DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->header.block_in_disk);
    if(res==FAILED) return FAILED;

    return SUCCESS;
  }
  return FAILED;
}

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int SimpleFS_remove(DirectoryHandle* d, const char* filename){
   // Check if passed parameters are valid
  if(d == NULL || strlen(filename)==0) return FAILED;

  int FDB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
  int dir_entries=d->dcb->num_entries;
  int searching=1, i, res;

  if(dir_entries==0) return SUCCESS;

  FirstDirectoryBlock* file=(FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));

  // we search for all the files in the current directory block
  for(i=0;i<dir_entries && i<FDB_max_elements && searching;i++){
    //we get the file
    memset(file, 0, sizeof(FirstDirectoryBlock));
    res=DiskDriver_readBlock(d->sfs->disk, file, d->dcb->file_blocks[i]);
    CHECK_ERR(res==FAILED,"the directory contains a free block in the entry list");

    //we check if the file has the same name of the file we want to create
    searching=strncmp(file->fcb.name, filename, strlen(filename));
  }

  // Check if the target file is a directory
  if(searching==0 && file->fcb.is_dir==DIRECTORY){
    // We populate the handle according to the content informations
    DirectoryHandle* handle=(DirectoryHandle*) malloc(sizeof(DirectoryHandle));
    memset(handle, 0, sizeof(DirectoryHandle));
    handle->sfs=d->sfs;
    handle->current_block=&(file->header);
    handle->dcb=file;
    handle->directory = d->dcb;
    handle->pos_in_dir=0;
    handle->pos_in_block=sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);

    res=SimpleFS_remove_rec(handle);
    if(res==FAILED){
      free(file);
      free(handle);
      return FAILED;
    }
    free(handle);

    // Remove the files contained in the directory
    int i;
    for(i=0; i<file->num_entries; i++){
      // TODO Free all block occupied that file
      DiskDriver_freeBlock(d->sfs->disk, file->file_blocks[i]);
      file->file_blocks[i]=MISSING;
    }
    file->num_entries-=i;

    // Remove that the directory in the parent directory
    int j=0;
    for(i=0; i<d->dcb->num_entries; i++){
      if(d->dcb->file_blocks[i]!=file->header.block_in_disk){
        d->dcb->file_blocks[j]=d->dcb->file_blocks[i];
        j++;
      }
      else{
        // Free all block occupied that directory
        // TODO CHECK IF IS CORRECT!
        if(file->header.block_in_file==CONTROL_BLOCK){
          int res, block = file->header.next_block;
          DirectoryBlock* app = malloc(sizeof(DirectoryBlock));
          while(block!=MISSING){
            memset(app, 0, sizeof(DirectoryBlock));
            res=DiskDriver_readBlock(d->sfs->disk, (void*)app, block);
            if(res==FAILED) return FAILED;
            res=DiskDriver_freeBlock(d->sfs->disk, block);
            if(res==FAILED) return FAILED;
            block = app->header.next_block;
          }
          free(app);
        }

        DiskDriver_freeBlock(d->sfs->disk ,d->dcb->file_blocks[i]);
        d->dcb->file_blocks[i]=MISSING;
      }
    }
    free(file);

    // Update num entry of directory
    d->dcb->num_entries=j;

    // We write the updated FirstDirectoryBlock
    res=DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->header.block_in_disk);
    if(res==FAILED) return FAILED;

    return SUCCESS;
  }

  // Check if the target file isn't a file
  if(searching==0 && file->fcb.is_dir==FILE){
    // Remove that file according to the entries
    int i, j=0;
    for(i=0; i<d->dcb->num_entries; i++){
      if(d->dcb->file_blocks[i]!=file->header.block_in_disk){
        d->dcb->file_blocks[j]=d->dcb->file_blocks[i];
        j++;
      }
      else{
        // TODO Free all block occupied that file

        DiskDriver_freeBlock(d->sfs->disk ,d->dcb->file_blocks[i]);
        d->dcb->file_blocks[i]=MISSING;
      }
    }
    free(file);
    // Update num entry of directory
    d->dcb->num_entries=j;

    // We write the updated FirstDirectoryBlock
    res=DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->header.block_in_disk);
    if(res==FAILED) return FAILED;

    return SUCCESS;
  }

  return FAILED;
}


FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename){
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
	file->current_index_block->block_in_disk=MISSING;
	//we have 0 beacuse our index is the fcb
	file->current_index_block->block_in_file=0;
	file->current_index_block->next_block=MISSING;
	file->current_index_block->previous_block=MISSING;
	// we cleanup the result of our search
	if(search->last_visited_dir_block!=NULL){
		free(search->last_visited_dir_block);
	}
	free(search);
	return file;
}

void SimpleFS_close(FileHandle* f){

	//we deallocate BlockHeader which indicates the current data block in the file
	if(f->current_block!=NULL){
		free(f->current_block);
	}
	//we deallocate the current BlockHeader of the index
	if(f->current_index_block!=NULL){
		free(f->current_index_block);
	}
	//we need to deallocate the FirstFileBlock
	free(f->fcb);
	//we deallocate the directory block in which the file is contained
	free(f->directory);
	//now we can deallocate the file handle
	free(f);
}

// we add a block in the index list of the file returning FAILED/SUCCESS
int SimpleFS_addIndex(FileHandle *f,int block){
	//firstly we try to fill the array in the FirstFileBlock
	int i,res;
	int FFB_max_entries=(BLOCK_SIZE-sizeof(FileControlBlock) - sizeof(BlockHeader)-sizeof(int)-sizeof(int))/sizeof(int);
	int IB_max_entries=(BLOCK_SIZE-sizeof(int)-sizeof(int)-sizeof(int));

	if(f->current_index_block->block_in_file==0){
		//we search for a free spot in the FFB
		for(i=0;i<FFB_max_entries;i++){
			if(f->fcb->blocks[i]!=MISSING){
				f->fcb->blocks[i]=block;
				f->fcb->num_entries++;
				res=DiskDriver_writeBlock(f->sfs->disk,f->fcb,f->fcb->header.block_in_disk);
				CHECK_ERR(res==FAILED,"can't write the update to disk");
				return SUCCESS;
			}
		}
		// if we get in here we need to allocate the 2nd index block
		int new_index=DiskDriver_getFreeBlock(f->sfs->disk,0);
		CHECK_ERR(new_index==FAILED,"can't get a new index block");
		Index *new_index_block=(Index*)malloc(sizeof(Index));
		new_index_block->block_in_disk=new_index;
		new_index_block->nextIndex=MISSING;
		new_index_block->previousIndex=f->current_index_block->block_in_disk;
		new_index_block->indexes[0]=block;
		for(i=1;i<IB_max_entries;i++){
			new_index_block->indexes[i]=MISSING;
		}

		//now we write the new index block to disk
		res=DiskDriver_writeBlock(f->sfs->disk,new_index_block,new_index);
		free(new_index_block);
		CHECK_ERR(res==FAILED,"can't write the 2nd index block");
		//we update the FFB
		f->fcb->next_IndexBlock=new_index;
		f->fcb->num_entries++;
		res=DiskDriver_writeBlock(f->sfs->disk,f->fcb,f->fcb->header.block_in_disk);
		CHECK_ERR(res==FAILED,"can't write the update to disk");
		return SUCCESS;
	}

	Index *index=(Index*)malloc(sizeof(Index));
	while(f->current_index_block->next_block!=MISSING){

		res=DiskDriver_readBlock(f->sfs->disk,index,f->current_index_block->next_block);
		if(res==FAILED){
			free(index);
			CHECK_ERR(res==FAILED,"can't read the next index block");
		}
		// we update our current index block
		f->current_index_block->block_in_file++;
		f->current_index_block->next_block=index->nextIndex;
		f->current_index_block->next_block=index->previousIndex;
		for(i=0;i<IB_max_entries;i++){
			if(index->indexes[i]!=MISSING){
				index->indexes[i]=block;
				res=DiskDriver_writeBlock(f->sfs->disk,index,index->block_in_disk);
				free(index);
				CHECK_ERR(res==FAILED,"can't write update to disk");
				return SUCCESS;
			}
		}
	}
	//if we get here we need to allocate a new index block
	int new_index=DiskDriver_getFreeBlock(f->sfs->disk,0);
	CHECK_ERR(new_index==FAILED,"can't get a new index block");
	Index *new_index_block=(Index*)malloc(sizeof(Index));
	new_index_block->block_in_disk=new_index;
	new_index_block->nextIndex=MISSING;
	new_index_block->previousIndex=f->current_index_block->block_in_disk;
	new_index_block->indexes[0]=block;
	for(i=1;i<IB_max_entries;i++){
		new_index_block->indexes[i]=MISSING;
	}
	//we update our current_index_block
	f->current_index_block->block_in_disk=new_index;
	f->current_index_block->next_block=MISSING;
	f->current_index_block->previous_block=new_index_block->previousIndex;
	//now we write the new index block to disk
	res=DiskDriver_writeBlock(f->sfs->disk,new_index_block,new_index);
	free(new_index_block);
	CHECK_ERR(res==FAILED,"can't write the next index block");
	//we update the actual index block
	index->nextIndex=new_index;
	res=DiskDriver_writeBlock(f->sfs->disk,index,f->current_index_block->previous_block);
	free(index);
	CHECK_ERR(res==FAILED,"can't write the actual index block");
	// we update the FFB
	f->fcb->num_entries++;
	res=DiskDriver_writeBlock(f->sfs->disk,f->fcb,f->fcb->header.block_in_disk);
	CHECK_ERR(res==FAILED,"can't write the update to disk");
	return SUCCESS;
}

//given the block num in the file it returns the block in disk/FAILED
int SimpleFS_getIndex(FileHandle *f,int block_in_file){
	int i,res,index_num,block_in_disk=FAILED;
	int FFB_max_entries=(BLOCK_SIZE-sizeof(FileControlBlock) - sizeof(BlockHeader)-sizeof(int)-sizeof(int))/sizeof(int);
	int IB_max_entries=(BLOCK_SIZE-sizeof(int)-sizeof(int)-sizeof(int));
	//firstly we need to get the number of the index which contains the block we need
	if(block_in_file<FFB_max_entries){
		index_num=0;
	}else{
		index_num=(block_in_file-FFB_max_entries)/IB_max_entries;
	}

	if(index_num==0){
		//we search in the FFB
		block_in_disk=f->fcb->blocks[block_in_file];
	}else{
		//if we get here we need to search in the other index blocks
		Index *index=(Index*) malloc(sizeof(Index));
		if(f->current_index_block->block_in_file>index_num){
			//we search for another index block after the current block
			for(i=0;i<index_num;i++){
				//we check if we have another index block
				if(f->current_index_block->next_block==MISSING){
					return FAILED;
				}
				//we read the index
				res=DiskDriver_readBlock(f->sfs->disk,index,f->current_index_block->next_block);
				if(res==FAILED){
					free(index);
					CHECK_ERR(res==FAILED,"can't read the next index block");
				}
			}
		}else{
			if(f->current_index_block->block_in_file<index_num){
				//we search for another index block before the current block
				for(i=0;i<index_num;i++){
					//we check if we have another index block
					if(f->current_index_block->previous_block==MISSING){
						return FAILED;
					}
					//we read the index
					res=DiskDriver_readBlock(f->sfs->disk,index,f->current_index_block->previous_block);
					if(res==FAILED){
						free(index);
						CHECK_ERR(res==FAILED,"can't read the next index block");
					}
				}
			}else{
				//if we get here we need to read only the current index block
				res=DiskDriver_readBlock(f->sfs->disk,index,f->current_index_block->previous_block);
				if(res==FAILED){
					free(index);
					CHECK_ERR(res==FAILED,"can't read the next index block");
				}
			}
		}
		//now we need to get the index from the found block
		block_in_disk=index->indexes[(block_in_file-FFB_max_entries)%IB_max_entries];
	}
	return block_in_disk;
}

// returns pos on success -1 on error (file too short)
int SimpleFS_seek(FileHandle* f, int pos){
	//we calculate the block in which we need to move
	int block_in_file=pos/BLOCK_SIZE;
	int res;
	//we check if the block we need exists by checking on the index
	int pos_in_disk=SimpleFS_getIndex(f,block_in_file);
	if(pos_in_disk==FAILED){
		//the block doesn't exists
		return FAILED;
	}else{
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

	//we must write on the current block first, but it could be the fcb
	int current_block_written=0;
	if(f->current_block->block_in_file!=0){
		current_block_written=1;
	}

	FileBlock* block=(FileBlock*)malloc(sizeof(FileBlock));

	while(blocks_needed>0 && res!=FAILED){
		displacement=f->pos_in_file%DB_max_elements;
		//we clear our block in memory to avoid writing cluttter on disk
		memset(block,0,sizeof(FileBlock));
		if(current_block_written){
			//in this case we are writing at the beginning of the file
			//or need a new block to keep writing
			res=DiskDriver_getFreeBlock(f->sfs->disk,f->current_block->block_in_disk);
			if(res!=FAILED){
				//we initialize the new block
				block->header.block_in_disk=res;
				block->header.next_block=MISSING;
				block->header.previous_block=f->current_block->block_in_disk;
				block->header.block_in_file=f->current_block->block_in_file+1;
				//we update our current_block
				memcpy(f->current_block,&(block->header),sizeof(BlockHeader));
			}
		}else{
			//if we get here we only need to load the block from disk
			res=DiskDriver_readBlock(f->sfs->disk,block,f->current_block->block_in_disk);
			//and we indicate that our current block will be written
			current_block_written=1;
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
			//we write the block on disk
			res=DiskDriver_writeBlock(f->sfs->disk,block,block->header.block_in_disk);
			//we update the cursor and the bytes written
			if(res!=FAILED){
				if(size-bytes_written>DB_max_elements-displacement){
					bytes_written+=DB_max_elements-displacement;
					f->pos_in_file+=DB_max_elements-displacement;
				}else{
					f->pos_in_file+=size-bytes_written;
					bytes_written+=size-bytes_written;
				}
				blocks_needed--;
			}
		}
	}
	free(block);
	return bytes_written;
}
