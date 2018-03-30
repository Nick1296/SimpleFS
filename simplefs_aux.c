#include "simplefs_aux.h"

// searches for a file or a directory given a DirectoryHandle and the element name
SearchResult* SimpleFS_search(DirectoryHandle* d, const char* name){
	//we check that we have been given valid data
	if(d==NULL || strlen(name)==0){
		return NULL;
	}

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
	int Dir_Block_max_elements=(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);

	// in here we will save the read FirstFileBlock/FirstDirectoryBlock to check if has a matching file name
	FirstFileBlock* element=malloc(sizeof(FirstFileBlock));
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
		searching=strncmp(element->fcb.name,name,strlen(name));
	}
	if(searching==0){
		result->result=SUCCESS;
		result->dir_block_in_disk=MISSING;
		result->type=element->fcb.is_dir;
		result->last_visited_dir_block=NULL;
		result->element=element;
		result->pos_in_block=i-1;
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
		for(i=0;i<dir_entries && i<Dir_Block_max_elements && searching;i++){
			//we get the file
			res=DiskDriver_readBlock(d->sfs->disk,element,dir->file_blocks[i]);
			if(res==FAILED){
				free(element);
				free(dir);
				free(result);
				CHECK_ERR(res==FAILED,"the directory contains a free block in the entry list");
			}

			//we check if the file has the same name of the file we want to create
			searching=strncmp(element->fcb.name,name,strlen(name));
		}
		if(searching==0){
			result->type=element->fcb.is_dir;
			result->dir_block_in_disk=dir_block_in_disk;
			result->element=element;
			result->last_visited_dir_block=dir;
			result->result=SUCCESS;
			result->pos_in_block=i;
		}
		//now we need to change block and to adjust the number of remaining entries to check
		dir_entries-=Dir_Block_max_elements;
		next_block=dir->header.next_block;
	}
	return result;
}

void Dir_addentry(DirectoryHandle *d,int file_block){
	//we check that we have been given valid data
	if(d==NULL || file_block<=0 || file_block>=d->sfs->disk->header->num_blocks){
		return ;
	}
	//we know how many entries has a directory but we don't know how many entries per block we have so we calculate it
	int FDB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
	int Dir_Block_max_elements=(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
	int res;
	//we use the current block, pos_in_block and pos_in_dir to determine the position where we
	//must write the new file in the directory
	//but it could be uninitialized!
	DirectoryBlock *dir=(DirectoryBlock*)malloc(sizeof(DirectoryBlock));
	//we determine if the current block is uninitialized
	if(d->current_block->next_block!=MISSING ){
		while(d->current_block->next_block!=MISSING){
			res=DiskDriver_readBlock(d->sfs->disk,dir,d->current_block->next_block);
			CHECK_ERR(res==FAILED,"can't read the directory block");
			memcpy(d->current_block,&(dir->header),sizeof(BlockHeader));
		}
	}else{
		//we check if we must load the current directory block
		if(d->current_block->block_in_file!=0){
			res=DiskDriver_readBlock(d->sfs->disk,dir,d->current_block->block_in_disk);
			CHECK_ERR(res==FAILED,"can't read the directory block");
		}
	}

	d->pos_in_dir=d->dcb->num_entries;
	if(d->current_block->block_in_file==0){
		d->pos_in_block=d->pos_in_dir-1;
	}else{
		d->pos_in_block=(d->pos_in_dir-FDB_max_elements-1)-Dir_Block_max_elements*(d->current_block->block_in_file-1);
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
			CHECK_ERR(new_dir_block==FAILED,"full disk on directory update");
			dir->header.block_in_file=d->dcb->header.block_in_file+1;
			dir->header.next_block=MISSING;
			dir->header.block_in_disk=new_dir_block;
			dir->header.previous_block=d->dcb->header.block_in_disk;
			//we set the chain to MISSING
			memset(dir->file_blocks,MISSING,sizeof(int)*Dir_Block_max_elements);
			dir->file_blocks[dir_block_displacement]=file_block;
			d->dcb->header.next_block=new_dir_block;
			//we update the current_block
			memcpy(d->current_block,&(dir->header),sizeof(BlockHeader));
			res=DiskDriver_writeBlock(d->sfs->disk,dir,new_dir_block);
			CHECK_ERR(res==FAILED,"can't write the 2nd directory block");
		}
		free(dir);
	}else{
		//we save the current block in disk because it could change
		int current_block=d->current_block->block_in_disk;
		// now we must check if in the last directory_block we have room for one more file
		if(dir_block_displacement<Dir_Block_max_elements){
			//in this case we can add the file directly in the DB
			dir->file_blocks[dir_block_displacement]=file_block;
		}else{
			int new_dir_block=DiskDriver_getFreeBlock(d->sfs->disk,0);
			CHECK_ERR(new_dir_block==FAILED,"full disk on directory update");
			dir_block_displacement=0;
			DirectoryBlock *new_dir=(DirectoryBlock*)malloc(sizeof(DirectoryBlock));
			new_dir->header.block_in_file=dir->header.block_in_file+1;
			new_dir->header.block_in_disk=new_dir_block;
			new_dir->header.next_block=MISSING;
			new_dir->header.previous_block=d->current_block->block_in_disk;
			//we set the chain to MISSING
			memset(new_dir->file_blocks,MISSING,sizeof(int)*Dir_Block_max_elements);
			new_dir->file_blocks[dir_block_displacement]=file_block;
			dir->header.next_block=new_dir_block;
			//we update the current_block
			memcpy(d->current_block,&(new_dir->header),sizeof(BlockHeader));
			//we write the new dir_block
			res=DiskDriver_writeBlock(d->sfs->disk,new_dir,new_dir_block);
			free(new_dir);
			if(res==FAILED){
				free(dir);
				CHECK_ERR(res==FAILED,"can't write the new dir_block on disk");
			}

		}
		//we write the updated dir_block
		res=DiskDriver_writeBlock(d->sfs->disk,dir,current_block);
		free(dir);
		CHECK_ERR(res==FAILED,"can't write the updated dir_block on disk");
	}
	//we update the metedata into the FDB
	d->dcb->num_entries++;

	//we update the metadata into the DirectoryHandle
	d->pos_in_dir++;
	d->pos_in_block=dir_block_displacement;
	//we write the updated FirstDirectoryBlock
	res=DiskDriver_writeBlock(d->sfs->disk,d->dcb,d->dcb->header.block_in_disk);
	CHECK_ERR(res==FAILED,"can't write the updated FirstDirectoryBlock on disk");
}

//remves one file, including the FCB, all the index block and all the indexes
int SimpleFS_removeFile(SimpleFS *sfs, int file){
	int i,res,stop=0;
	int FFB_max_entries=(BLOCK_SIZE-sizeof(FileControlBlock) - sizeof(BlockHeader)-sizeof(int)-sizeof(int))/sizeof(int);
	int IB_max_entries=(BLOCK_SIZE-sizeof(int)-sizeof(int)-sizeof(int))/sizeof(int);
	int next_IndexBlock;
	//we load our FFB
	FirstFileBlock *ffb=(FirstFileBlock*)malloc(sizeof(FirstFileBlock));
	res=DiskDriver_readBlock(sfs->disk,ffb,file);
	if(res==FAILED){
		free(ffb);
		CHECK_ERR(res==FAILED,"can't read the FFB to remove");
	}
	//firstly we deallocate the blocks indexed in our FileControlBlock
	for(i=0;i<FFB_max_entries && !stop;i++){
		if(ffb->blocks[i]!=MISSING){
			res=DiskDriver_freeBlock(sfs->disk,ffb->blocks[i]);
			CHECK_ERR(res==FAILED,"can't deallocate a data block indexed in the ffb");
		}else{
			stop=1;
		}
	}
	//now we need to deallocate the ffb
	res=DiskDriver_freeBlock(sfs->disk,file);
	//now before destroying our ffb in memory we check if there are other IndexBlocks
	if(ffb->next_IndexBlock==MISSING){
		free(ffb);
		return SUCCESS;
	}
	//if we get here we need to deallocate the other IndexBlocks
	//we load the first IndexBlock
	Index* index=(Index*)malloc(sizeof(Index));                                     ////////////////////////////////////////////////////////////////////////////////////
    next_IndexBlock=ffb->next_IndexBlock;
	//now we can deallocate the ffb
	free(ffb);
	//now we deallocate all the data blocks in the index chain
	while(next_IndexBlock!=MISSING){
		//if possible we load the next index block
		memset(index,0,sizeof(Index));
		res=DiskDriver_readBlock(sfs->disk,index,next_IndexBlock);                  /////////////////////// ho la index popolata
                                                                                    //block in file dovrebbe gia essere inizializzata  ////////////////
		CHECK_ERR(res==FAILED,"can't load the next index block");
		//we deallocate all the data block in this index block
		for(i=0;i<IB_max_entries && !stop;i++){
			if(index->indexes[i]!=MISSING){
				res=DiskDriver_freeBlock(sfs->disk,index->indexes[i]);
				// TODO there's problem here!
				CHECK_ERR(res==FAILED,"can't deallocate a block in the index chain");
			}else{
				stop=1;
			}
		}
		//now we deallocate the current index block
		res=DiskDriver_freeBlock(sfs->disk,index->header.block_in_disk);       ////////////////////////////////////////////////////////////////////////////////////
		CHECK_ERR(res==FAILED,"can't deallocate the current index block");
		//we get the next index block
		next_IndexBlock=index->header.next_block;                               ////////////////////////////////////////////////////////////////////////////////////
	}
	free(index);
	return SUCCESS;
}

int SimpleFS_removeChildDir(DirectoryHandle* handle){
	int FDB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
	int Dir_Block_max_elements=(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
	FirstDirectoryBlock* fdb=(FirstDirectoryBlock*) malloc(sizeof(FirstDirectoryBlock));
	DirectoryBlock* db = (DirectoryBlock*)malloc(sizeof(DirectoryBlock));
	DirectoryBlock* db1 = (DirectoryBlock*)malloc(sizeof(DirectoryBlock));
	int i, z, res, pos=0;

	// Remove passed directory in the parent directory
	for(z=0; z<handle->directory->num_entries; z++){
		if(z < FDB_max_elements){
			if(handle->directory->file_blocks[z]==handle->dcb->header.block_in_disk){
				// free block target
				res=DiskDriver_freeBlock(handle->sfs->disk, handle->dcb->header.block_in_disk);
				CHECK_ERR(res==FAILED, "Error on freeBlock in simpleFS_remove");

				// Obtain the fdb of parent directory
				res=DiskDriver_readBlock(handle->sfs->disk , fdb, handle->directory->header.block_in_disk);
				CHECK_ERR(res==FAILED, "Error on read of FirstDirectoryBlock in simpleFS_remove");
				if(handle->directory->num_entries > FDB_max_elements){
					// search the last directoryBlock used
					int app = fdb->header.next_block;
					while(app!=MISSING){
						res=DiskDriver_readBlock(handle->sfs->disk , db, app);
						CHECK_ERR(res==FAILED, "Error on read of DirectoryBlock in simpleFS_remove");
						app = db->header.next_block;
					}
					// Trovo l'ultimo blocco occupato
					for(i=0; i<Dir_Block_max_elements && db->file_blocks[i]!=MISSING; i++);
					handle->directory->file_blocks[z]=db->file_blocks[i-1];
					db->file_blocks[i-1]=MISSING;

					// We write the updated on file
					res=DiskDriver_writeBlock(handle->sfs->disk, db, db->header.block_in_disk);
					if(res==FAILED) return FAILED;

				}else{
					// search the last item in file_block of directory block
					for(i=0; i<handle->directory->num_entries && handle->dcb->file_blocks[i]!=MISSING; i++);
					res=DiskDriver_freeBlock(handle->sfs->disk, handle->directory->header.block_in_disk);
					if(i-1!=0)handle->directory->file_blocks[z]=handle->directory->file_blocks[i-1];
					handle->directory->file_blocks[i-1]=MISSING;
				}
				handle->directory->num_entries--;

				// We write the updated on file
				res=DiskDriver_writeBlock(handle->sfs->disk, db, db->header.block_in_disk);
				if(res==FAILED) return FAILED;
			}
		}
		else{
			// File contenuto nei DB della directory genitore
			int logblock=(z/Dir_Block_max_elements)+1;
			int block = handle->directory->header.next_block;
			for(i=0; i<logblock && pos!=logblock; i++){
				res=DiskDriver_readBlock(handle->sfs->disk , db, block);
				CHECK_ERR(res==FAILED, "Error on read of DirectoryBlock in simpleFS_remove");
				block = db->header.next_block;
				pos=i;
			}
			if(db->file_blocks[(z-FDB_max_elements)%Dir_Block_max_elements]==handle->dcb->header.block_in_disk){
				// search the last directoryBlock used				
				int app = fdb->header.next_block;
				while(app!=MISSING){
					res=DiskDriver_readBlock(handle->sfs->disk , db, app);
					CHECK_ERR(res==FAILED, "Error on read of DirectoryBlock in simpleFS_remove");
					app = db->header.next_block;
				}
				// search the last item in file_block of directory block
				for(i=0; i<handle->directory->num_entries && db1->file_blocks[i]!=MISSING; i++);
				res=DiskDriver_freeBlock(handle->sfs->disk, handle->dcb->header.block_in_disk);
				CHECK_ERR(res==FAILED, "Error on freeBlock in simpleFS_remove");
				db->file_blocks[(z-FDB_max_elements)%Dir_Block_max_elements]=db1->file_blocks[i-1];
				db1->file_blocks[i-1]=MISSING;

				// We write the updated on file
				res=DiskDriver_writeBlock(handle->sfs->disk, db, db->header.block_in_disk);
				if(res==FAILED) return FAILED;
				res=DiskDriver_writeBlock(handle->sfs->disk, db1, db1->header.block_in_disk);
				if(res==FAILED) return FAILED;
			}
		}
	}
	free(db1);
	free(db);
	free(fdb);
	return SUCCESS;
}

int SimpleFS_removeFileOnDir(DirectoryHandle* dh, void* element, int pos_in_block){
		// Ricompatto la lista dei file contenuti nella directory
		// genitore del file
		int i, res, pos=0;
		int FDB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
		int Dir_Block_max_elements=(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
		DirectoryHandle* dir = dh;
		DirectoryBlock* db = (DirectoryBlock*)malloc(sizeof(DirectoryBlock));
		DirectoryBlock* db1 = (DirectoryBlock*)malloc(sizeof(DirectoryBlock));
		FirstFileBlock* file = (FirstFileBlock*)element;
		// Rimuovo il file
		SimpleFS_removeFile(dh->sfs, file->header.block_in_disk);

		if(pos_in_block<FDB_max_elements){
				// free the target block
				res=DiskDriver_freeBlock(dh->sfs->disk, dir->dcb->file_blocks[pos_in_block]);
				CHECK_ERR(res==FAILED, "Error on freeBlock in simpleFS_remove");

				// File contenuto nell'FDB della directory genitore
				if(dir->dcb->num_entries >= FDB_max_elements){
					memset(db, 0, sizeof(DirectoryBlock));
					// TODO did this iteration below because currentBlock does not assume the values it should have. check with gdb
					int app = dir->dcb->header.next_block;
					while(app!=MISSING){
						res=DiskDriver_readBlock(dh->sfs->disk , db, app);
						CHECK_ERR(res==FAILED, "Error on read of DirectoryBlock in simpleFS_remove");
						app = db->header.next_block;
					}
					// search the last item in file_block of directory block
					for(i=0; i<Dir_Block_max_elements && db->file_blocks[i]!=MISSING; i++);
					dir->dcb->file_blocks[pos_in_block]=db->file_blocks[i-1];
					db->file_blocks[i-1]=MISSING;

					// We write the updated on file
					res=DiskDriver_writeBlock(dh->sfs->disk, db, db->header.block_in_disk);
					if(res==FAILED) return FAILED;
				}else{
					// search the last item in file_block of directory block
					for(i=0; i<dir->dcb->num_entries && dir->dcb->file_blocks[i]!=MISSING; i++);
					if(i-1!=0) dir->dcb->file_blocks[pos_in_block]=dir->dcb->file_blocks[i-1];
					dir->dcb->file_blocks[i-1]=MISSING;
				}
				dir->dcb->num_entries--;

				// We write the updated on file
				res=DiskDriver_writeBlock(dh->sfs->disk, dir, dir->dcb->header.block_in_disk);
				if(res==FAILED) return FAILED;
		}else{
			// File contenuto nei DB della directory genitore
			int logblock=((pos_in_block-FDB_max_elements)/Dir_Block_max_elements)+1;
			int block = dir->dcb->header.next_block;
			for(i=0; i<logblock && pos!=logblock; i++){
				memset(db, 0, sizeof(DirectoryBlock));
				res=DiskDriver_readBlock(dh->sfs->disk , db, block);
				CHECK_ERR(res==FAILED, "Error on read of DirectoryBlock in simpleFS_remove");
				block = db->header.next_block;
				pos=i+1;
			}
			memset(db1, 0, sizeof(DirectoryBlock));
			// TODO did this iteration below because currentBlock does not assume the values it should have. check with gdb
			int app = dh->dcb->header.next_block;
			while(app!=MISSING){
				res=DiskDriver_readBlock(dh->sfs->disk , db1, app);
				CHECK_ERR(res==FAILED, "Error on read of DirectoryBlock in simpleFS_remove");
				app = db1->header.next_block;
			}
			// Trovo l'ultimo blocco occupato
			for(i=0; i<Dir_Block_max_elements && db1->file_blocks[i]!=MISSING; i++);
			res=DiskDriver_freeBlock(dh->sfs->disk,db->file_blocks[(pos_in_block-FDB_max_elements)%Dir_Block_max_elements]);
			CHECK_ERR(res==FAILED, "Error on freeBlock in simpleFS_remove");
			db->file_blocks[pos_in_block]=db1->file_blocks[i-1];
			db1->file_blocks[i-1]=MISSING;

			// We write the updated on file
			res=DiskDriver_writeBlock(dh->sfs->disk, db, db->header.block_in_disk);
			if(res==FAILED) return FAILED;
			res=DiskDriver_writeBlock(dh->sfs->disk, db1, db1->header.block_in_disk);
			if(res==FAILED) return FAILED;
		}
		free(db1);
		free(db);

		return SUCCESS;
}

// Removes files and directories recursively
int SimpleFS_remove_rec(DirectoryHandle* handle){
   // Check if passed parameters are valid
  if(handle == NULL) return FAILED;

  int FDB_max_elements=(BLOCK_SIZE-sizeof(BlockHeader)-sizeof(FileControlBlock)-sizeof(int))/sizeof(int);
	int Dir_Block_max_elements=(BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int);
  int i, res;

	// Remove the files contained in the directory
	DirectoryBlock* db = (DirectoryBlock*)malloc(sizeof(DirectoryBlock));
	FirstDirectoryBlock* fh = (FirstDirectoryBlock*)malloc(sizeof(FirstDirectoryBlock));
	DirectoryHandle* hand = (DirectoryHandle*)malloc(sizeof(DirectoryBlock));
	int z, pos=0;

	for(z=0; z<handle->dcb->num_entries; z++){
		memset(fh, 0, sizeof(FirstDirectoryBlock));
		if(z < FDB_max_elements){
			res=DiskDriver_readBlock(handle->sfs->disk, fh, handle->dcb->file_blocks[z]);
			CHECK_ERR(res==FAILED, "Error read dir block in simplefs_remove");
			if(fh->fcb.is_dir==FILE) SimpleFS_removeFileOnDir(handle, (void*)fh, z);
			else{
				memset(hand, 0, sizeof(DirectoryHandle));
				hand->sfs=handle->sfs;
				hand->dcb=fh;
				hand->directory=handle->dcb;
				hand->current_block = &(fh->header);
				hand->pos_in_block=0;
				hand->pos_in_dir=0;

				if(hand->dcb->num_entries==0){
					res=DiskDriver_freeBlock(handle->sfs->disk, hand->dcb->header.block_in_disk);
					CHECK_ERR(res==FAILED, "Failed to free block in simplefs_remove_rec");
				}else SimpleFS_remove_rec(hand);
			}
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
			else{
				DirectoryHandle* hand = (DirectoryHandle*)malloc(sizeof(DirectoryBlock));
				memset(hand, 0, sizeof(DirectoryHandle));
				hand->sfs=handle->sfs;
				hand->dcb=fh;
				hand->directory=handle->dcb;
				hand->current_block = &(fh->header);
				hand->pos_in_block=0;
				hand->pos_in_dir=0;

				if(hand->dcb->num_entries==0){
					res=DiskDriver_freeBlock(handle->sfs->disk, hand->dcb->header.block_in_disk);
					CHECK_ERR(res==FAILED, "Failed to free block in simplefs_remove_rec");
				}else SimpleFS_remove_rec(hand);
			}
		}
	}
	free(hand);
	free(fh);
	free(db);

	res=SimpleFS_removeChildDir(handle);
	CHECK_ERR(res==FAILED, "Error in remove a child dir");

  return SUCCESS;
}

// we add a block in the index list of the file returning FAILED/SUCCESS
int SimpleFS_addIndex(FileHandle *f,int block){
	//we check that we have been given valid data
	if(f==NULL  || block<=0 || block>=f->sfs->disk->header->num_blocks){
		return FAILED;
	}

	int i,res;
	int FFB_max_entries=(BLOCK_SIZE-sizeof(FileControlBlock) - sizeof(BlockHeader)-sizeof(int)-sizeof(int))/sizeof(int);
	int IB_max_entries=(BLOCK_SIZE-sizeof(int)-sizeof(int)-sizeof(int))/sizeof(int);
	
	//we use he current_index block to determine where we must put the new block
	if(f->fcb->num_entries-FFB_max_entries<=0){
		//we search for a free spot in the FFB
		for(i=0;i<FFB_max_entries;i++){
			if(f->fcb->blocks[i]==MISSING){
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
		Index *new_index_block=(Index*)malloc(sizeof(Index));                         ////////////////////////////////////////////////////////////////////////////////////
		new_index_block->header.block_in_disk=new_index;                                ////////////////////////////////////////////////////////////////////////////////////
		new_index_block->header.next_block=MISSING;                                     ////////////////////////////////////////////////////////////////////////////////////
		new_index_block->header.previous_block=f->current_index_block->block_in_disk;  ////////////////////////////////////////////////////////////////////////////////////
        new_index_block->header.block_in_file=f->current_index_block->block_in_file +1;/////////////////////////////////////////////////////////////////////////////////////
		new_index_block->indexes[0]=block;
		for(i=1;i<IB_max_entries;i++){
			new_index_block->indexes[i]=MISSING;
		}

		//we update our current index block
		f->current_index_block->block_in_disk=new_index_block->header.block_in_disk;
		f->current_index_block->next_block=new_index_block->header.next_block;   //////////////////////////////////////////////////////////////////////////////////////////////////
		f->current_index_block->previous_block=new_index_block->header.previous_block; /////////////////////////////////////////////////////////////////////////////

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

	Index *index=(Index*)malloc(sizeof(Index));                                     ////////////////////////////////////////////////////////////////////////////////////
	//if we get here we use the index block in current_index_block to add the index
	res=DiskDriver_readBlock(f->sfs->disk,index,f->current_index_block->block_in_disk);
                                                                                    //block_in_file gia popolata in teoria
	CHECK_ERR(res==FAILED,"can't load the current index block from disk");
	for(i=0;i<IB_max_entries;i++){
		if(index->indexes [i]==MISSING){
			index->indexes[i]=block;
			f->fcb->num_entries++;
			res=DiskDriver_writeBlock(f->sfs->disk,index,index->header.block_in_disk); ////////////////////////////////////////////////////////////////////////////////////
			free(index);
			CHECK_ERR(res==FAILED,"can't write the update to the current index block on disk");
			res=DiskDriver_writeBlock(f->sfs->disk,f->fcb,f->fcb->header.block_in_disk);
			CHECK_ERR(res==FAILED,"can't write the update to disk");
			return SUCCESS;
		}
	}
	//if we get here we need to allocate a new index block
	int new_index=DiskDriver_getFreeBlock(f->sfs->disk,0);
	CHECK_ERR(new_index==FAILED,"can't get a new index block");
	Index *new_index_block=(Index*)malloc(sizeof(Index));               ////////////////////////////////////////////////////////////////////////////////////
	new_index_block->header.block_in_disk=new_index;                   ////////////////////////////////////////////////////////////////////////////////////
	new_index_block->header.next_block=MISSING;
	new_index_block->header.previous_block=f->current_index_block->block_in_disk;      ////////////////////////////////////////////////////////////////////////////
    new_index_block->header.block_in_file=f->current_index_block->block_in_file +1; ////////////////////////////////////////////////////////////////////////////
	new_index_block->indexes[0]=block;
	for(i=1;i<IB_max_entries;i++){
		new_index_block->indexes[i]=MISSING;
	}

	//we update our current_index_block
	f->current_index_block->block_in_disk=new_index;
	f->current_index_block->next_block=MISSING;
	f->current_index_block->previous_block=new_index_block->header.previous_block;
	//now we write the new index block to disk
	res=DiskDriver_writeBlock(f->sfs->disk,new_index_block,new_index);
	free(new_index_block);
	CHECK_ERR(res==FAILED,"can't write the next index block");
	//we update the actual index block
	index->header.next_block=new_index;
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
	//we check that we have been given valid data
	if(f==NULL || block_in_file<0){
		return FAILED;
	}

	int i,res,index_num,block_in_disk=FAILED;
	int FFB_max_entries=(BLOCK_SIZE-sizeof(FileControlBlock) - sizeof(BlockHeader)-sizeof(int)-sizeof(int))/sizeof(int);
	int IB_max_entries=(BLOCK_SIZE-sizeof(int)-sizeof(int)-sizeof(int))/sizeof(int);
	//firstly we need to get the number of the index which contains the block we need
	if(block_in_file<FFB_max_entries){
		index_num=0;
	}else{
		//TODO check!
		index_num=((block_in_file-FFB_max_entries)/IB_max_entries)+1;
	}

	if(index_num==0){
		//we search in the FFB
		block_in_disk=f->fcb->blocks[block_in_file];
	}else{
		//if we get here we need to search in the other index blocks
		Index *index=(Index*) malloc(sizeof(Index));
		if(f->current_index_block->block_in_file<index_num){
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
				//we update the current index block
				f->current_index_block->block_in_disk=index->header.block_in_disk;
				f->current_index_block->next_block=index->header.next_block;
				f->current_index_block->previous_block=index->header.previous_block;
			}
		}else{
			if(f->current_index_block->block_in_file>index_num){
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
					//we update the current index block
					f->current_index_block->block_in_disk=index->header.block_in_disk;
					f->current_index_block->next_block=index->header.next_block;
					f->current_index_block->previous_block=index->header.previous_block;
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
		block_in_disk=index->indexes[(block_in_file-FFB_max_entries)-IB_max_entries*(f->current_index_block->block_in_file-1)];
		free(index);
	}
	return block_in_disk;
}

// this function clears the index list starting from the provided block in file
void SimpleFS_clearIndexes(FileHandle* f,int block_in_file){
	//we check that we have been given valid data
	if(f==NULL || block_in_file<=0){
		return;
	}
	int i,res,stop=0;
	int displacement;
	int FFB_max_entries=(BLOCK_SIZE-sizeof(FileControlBlock) - sizeof(BlockHeader)-sizeof(int)-sizeof(int))/sizeof(int);
	int IB_max_entries=(BLOCK_SIZE-sizeof(int)-sizeof(int)-sizeof(int))/sizeof(int);

	//we seek the block to adjust the current_index_block
	res=SimpleFS_getIndex(f,block_in_file);
	CHECK_ERR(res==FAILED,"can't clear find the first block to clear from indexes");
	Index *indexblock=(Index*)malloc(sizeof(Index));                    ////////////////////////////////////////////////////////////////////////////
                                                                        //c'è la read dovrebbe essere apposto block_in_file ///////////////////////////
	//if we have the index in the FFB we start clearing from there
	if(f->fcb->num_entries-FFB_max_entries<=0){
		displacement=(block_in_file%FFB_max_entries)-1;
		for(i=displacement;i<FFB_max_entries && !stop;i++){
			if(f->fcb->blocks[i]==MISSING){
				stop=1;
			}else{
				res=DiskDriver_freeBlock(f->sfs->disk,f->fcb->blocks[i]);
				CHECK_ERR(res==FAILED,"can't free a block from disk");
				f->fcb->blocks[i]=MISSING;
			}
		}
	}else{
		//we load the current index block
		res=DiskDriver_readBlock(f->sfs->disk,indexblock,f->current_index_block->block_in_disk);
		CHECK_ERR(res==FAILED,"can't load the current index block");
		displacement=block_in_file%IB_max_entries;
		for(i=displacement;i<IB_max_entries && !stop;i++){
			if(indexblock->indexes[i]==MISSING){
				stop=1;
			}else{
				res=DiskDriver_freeBlock(f->sfs->disk,indexblock->indexes[i]);
				CHECK_ERR(res==FAILED,"can't free a block from disk");
				indexblock->indexes[i]=MISSING;
			}
		}
		//we will remove any index blocks after the current block
		indexblock->header.next_block=MISSING;
		//we write the update on disk
		res=DiskDriver_writeBlock(f->sfs->disk,indexblock,indexblock->header.block_in_disk);  ////////////////////////////////////////////////////////////////////////
		CHECK_ERR(res==FAILED,"can't write the index block on disk");
	}
	//now we update the number of entries
	if(f->fcb->num_entries-FFB_max_entries<=0){
		f->fcb->num_entries=displacement-1;
	}else{
		f->fcb->num_entries=f->current_index_block->block_in_file*IB_max_entries+displacement-1;
	}

	//now we clear the eventual next index block which aren't used
	while(f->current_index_block->next_block!=MISSING){
		res=DiskDriver_readBlock(f->sfs->disk,indexblock,f->current_index_block->next_block);
		CHECK_ERR(res==FAILED,"can't load the current index block");
		for(i=0;i<IB_max_entries;i++){
			if(indexblock->indexes[i]!=MISSING){
			res=DiskDriver_freeBlock(f->sfs->disk,indexblock->indexes[i]);
			CHECK_ERR(res==FAILED,"can't free a block from disk");
			}
		}
		res=DiskDriver_freeBlock(f->sfs->disk,indexblock->header.block_in_disk);       /////////////////////////////////////////////////////////////////////
		CHECK_ERR(res==FAILED,"can't deallocate the index block")
		//we advance our current_index_block
		f->current_index_block->block_in_disk=indexblock->header.block_in_disk;
		f->current_index_block->next_block=indexblock->header.next_block;
		f->current_index_block->previous_block=indexblock->header.previous_block;
	}
	free(indexblock);
}
