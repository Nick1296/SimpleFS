#pragma once
#include "simplefs.h"

// searches for a file or a directory given a DirectoryHandle and the element name
SearchResult* SimpleFS_search(DirectoryHandle* d, const char* name);

// add a new entry in a directory structure
void Dir_addentry(DirectoryHandle *d,int file_block);

// removes a file on disk 
int SimpleFS_removeFile(SimpleFS *sfs, int file);

// removes passed directory from parent directory
int SimpleFS_removeChildDir(DirectoryHandle* handle);

// removes a file in directory structure
int SimpleFS_removeFileOnDir(DirectoryHandle* dh, void* element, int pos_in_block);

// removes files and directories recursively
int SimpleFS_remove_rec(DirectoryHandle* handle);

// we add a block in the index list of the file returning FAILED/SUCCESS
int SimpleFS_addIndex(FileHandle *f,int block);

//given the block num in the file it returns the block in disk/FAILED
int SimpleFS_getIndex(FileHandle *f,int block_in_file);

// this function clears the index list starting from the provided block in file
void SimpleFS_clearIndexes(FileHandle* f,int block_in_file);
