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
  int res;
	//we know how many entries has a directory but we don't know how many entries per block we have se we calculate it
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
  file->fcb.block_in_disk=file_block;
  file->fcb.directory_block=d->dcb->fcb.block_in_disk;
  strncpy(file->fcb.name,filename,strlen(filename));
  //i'm probably committing some errors in here
  // TODO CHECK!
  file->fcb.size_in_blocks=1;
  file->fcb.size_in_bytes=512;

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
		//TODO get from the SearchResult the last visited directory block
		DirectoryBlock* dir=search->last_visited_dir_block;
    //in this case we have at least a DirectoryBlock
    if(dir->header.block_in_file==dir_block){
			if(dir->file_blocks[dir_block_displacement]!=MISSING){
				free(file);
				free(dir);
				free(search);
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
      new_dir->header.previous_block=search->dir_block_in_disk;
      new_dir->file_blocks[0]=file_block;
      dir->header.next_block=new_dir_block;
      //we write the new dir_block
      res=DiskDriver_writeBlock(d->sfs->disk,&new_dir,new_dir_block);
			free(new_dir);
			if(res==FAILED){
				free(file);
				free(dir);
				free(search);
				CHECK_ERR(res==FAILED,"can't write the new dir_block on disk");
			}

    }
    //we write the updated dir_block
    res=DiskDriver_writeBlock(d->sfs->disk,dir,search->dir_block_in_disk);
		if(res==FAILED){
			free(file);
			free(dir);
			free(search);
			CHECK_ERR(res==FAILED,"can't write the updated dir_block on disk");
		}

		//now we can safely deallocate dir
		free(dir);

  }else{
    // in this case we must update only the FirstDirectoryBlock and on the worst case create a new block
		//so we can deallocate the SearchResult
		free(search);
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
				free(search);
			}
      CHECK_ERR(res==FAILED,"can't write the new dir_block on disk");
    }
  }
  d->dcb->num_entries+=1;
  //we write the updated FirstDirectoryBlock
  res=DiskDriver_writeBlock(d->sfs->disk,d->dcb,d->dcb->fcb.block_in_disk);
	if(res==FAILED){
		free(file);
	}
  CHECK_ERR(res==FAILED,"can't write the updated FirstDirectoryBlock on disk");
  //after creating the file we create a file handle
	FileHandle *fh=(FileHandle*)malloc(sizeof(FileHandle));
  fh->current_block=NULL;
	fh->current_index_block=NULL;
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
int SimpleFS_changeDir(DirectoryHandle* d, char* dirname){
  // Check if passed parameters are valid
  if(d == NULL || strlen(dirname)==0) return FAILED;

  // Strings with specific meaning
  const char parent[] = "..";
  const char workDir[] = ".";
  int res;

  // Check if the directory where want to move is the current workdir
  if(strncmp(workDir, dirname, strlen(dirname))==0) return SUCCESS;

  // Check if the directory where want to move is the parent directory
  if(strncmp(parent, dirname, strlen(dirname))==0){
    // Check if workdir is root dir
    if(d->directory==NULL) return SUCCESS;

    // We populate the handle according to the content informations
    DirectoryHandle* handle=(DirectoryHandle*) malloc(sizeof(DirectoryHandle));
    handle->sfs=d->sfs;
    handle->current_block=&(d->directory->header);
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
    d->current_block=handle->current_block;
    d->dcb=handle->dcb;
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
    handle->current_block=&(file->header);
    handle->dcb=file;
    handle->pos_in_dir=0;
    handle->pos_in_block=sizeof(BlockHeader)+sizeof(FileControlBlock)+sizeof(int);

    // Check if directory have parent directory
    if(file->fcb.directory_block!=MISSING){
      handle->directory = malloc(sizeof(FirstDirectoryBlock));
      memset(handle->directory, 0, sizeof(FirstDirectoryBlock));
      res=DiskDriver_readBlock(d->sfs->disk, handle->directory, file->fcb.directory_block);
      CHECK_ERR(res==FAILED,"Error read parent directory");
    }
    else handle->directory = NULL;

    free(d->dcb);

    // Copy the calculated information
    d->sfs = handle->sfs;
    d->current_block=handle->current_block;
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
int SimpleFS_mkDir(DirectoryHandle* d, char* dirname){
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
  root->fcb.directory_block=d->dcb->fcb.block_in_disk;
  root->fcb.block_in_disk=rootblock;
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

  res=DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb.block_in_disk);
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
      if(d->dcb->file_blocks[i]!=file->fcb.block_in_disk){
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
    res=DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb.block_in_disk);
    if(res==FAILED) return FAILED;

    return SUCCESS;
  }
  return FAILED;
}

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int SimpleFS_remove(DirectoryHandle* d, char* filename){
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
      if(d->dcb->file_blocks[i]!=file->fcb.block_in_disk){
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
    res=DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb.block_in_disk);
    if(res==FAILED) return FAILED;

    return SUCCESS;
  }

  // Check if the target file isn't a file
  if(searching==0 && file->fcb.is_dir==FILE){
    // Remove that file according to the entries
    int i, j=0;
    for(i=0; i<d->dcb->num_entries; i++){
      if(d->dcb->file_blocks[i]!=file->fcb.block_in_disk){
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
    res=DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->fcb.block_in_disk);
    if(res==FAILED) return FAILED;

    return SUCCESS;
  }
  
  return FAILED;
}

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
	dir_block_in_disk=d->dcb->fcb.block_in_disk;
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
	return result;
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
	file->current_block=NULL;
	file->current_index_block=NULL;

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
