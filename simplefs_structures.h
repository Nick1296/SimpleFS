#pragma once

#include "common.h"
#include "disk_driver_structures.h"
/*these are structures stored on disk*/

// header, occupies the first portion of each block in the disk
// represents a chained list of blocks
typedef struct _BlockHeader {
	int previous_block;   // chained list (previous block)
	int next_block;       // chained list (next_block)
	int block_in_file;    // position in the file, if 0 we have a file control block
	int block_in_disk;    // repeated position of the block on the disk
} BlockHeader;


/* permissions associated with every file in the disk, even directories
 these permission are divided in three groups: the owner (identified by its uid), the users in th group associated
 with the owner and all other users
 the permission is an octal number made by 3 bit which represent (from the MSB) read-write-execute permissions
the default permissions are 7 for owner, 4 for the group and the others */
typedef struct _Permissions {
	unsigned user_uid;
	unsigned group_uid; //TODO: check if it's correct to store the group uid in the file
	uint8_t user;
	uint8_t group;
	uint8_t others;
} Permissions;

// this is in the first block of a chain, after the header
typedef struct _FileControlBlock {
	Permissions permissions;
	int directory_block; // first block of the parent directory
	char name[FILENAME_MAX_LENGTH];
	int size_in_bytes;
	int size_in_blocks;
	int is_dir;          // 0 for file, 1 for dir
} FileControlBlock;


/******************* stuff on disk BEGIN *******************/
// this is the first physical block of a file
// it has a header
// an FCB storing file infos
// and can contain some indexes
typedef struct _FirstFileBlock {
	BlockHeader header;
	FileControlBlock fcb;
	int num_entries;
	int next_IndexBlock;
	int blocks[(BLOCK_SIZE - sizeof(FileControlBlock) - sizeof(BlockHeader) - sizeof(int) - sizeof(int)) / sizeof(int)];
} FirstFileBlock;

typedef struct _Index {
	BlockHeader header;
	int indexes[(BLOCK_SIZE - sizeof(BlockHeader)) / sizeof(int)];
} Index;
// this is one of the next physical blocks of a file
typedef struct _FileBlock {
	BlockHeader header;
	char data[BLOCK_SIZE - sizeof(BlockHeader)];
} FileBlock;

// this is the first physical block of a directory
typedef struct _FirstDirectoryBlock {
	BlockHeader header;
	FileControlBlock fcb;
	int num_entries;
	int file_blocks[(BLOCK_SIZE
	                 - sizeof(BlockHeader)
	                 - sizeof(FileControlBlock)
	                 - sizeof(int)) / sizeof(int)];
} FirstDirectoryBlock;

// this is remainder block of a directory
typedef struct _DirectoryBlock {
	BlockHeader header;
	int file_blocks[(BLOCK_SIZE - sizeof(BlockHeader)) / sizeof(int)];
} DirectoryBlock;
/******************* stuff on disk END *******************/

typedef struct _SimpleFS {
	DiskDriver *disk;
	int block_num;
	char *filename;
} SimpleFS;

// this is a file handle, used to refer to open files
typedef struct _FileHandle {
	SimpleFS *sfs;                   // pointer to memory file system structure
	FirstFileBlock *fcb;             // pointer to the first block of the file(read it)
	FirstDirectoryBlock *directory;  // pointer to the directory where the file is stored
	BlockHeader *current_block;      // current block in the file
	BlockHeader *current_index_block; // current block in the where the current block is saved
	int pos_in_file;                 // position of the cursor
} FileHandle;

typedef struct _DirectoryHandle {
	SimpleFS *sfs;                   // pointer to memory file system structure
	FirstDirectoryBlock *dcb;        // pointer to the first block of the directory(read it)
	FirstDirectoryBlock *directory;  // pointer to the parent directory (null if top level)
	BlockHeader *current_block;      // current block in the directory
	int pos_in_dir;                  // absolute position of the cursor in the directory
	int pos_in_block;                // relative position of the cursor in the block
} DirectoryHandle;

//result of a search operation in a given directory
//the file or directory blocks, if missing are NULLs
typedef struct _SearchResult {
	int result; //SUCCESS or FAILED
	int type; // 0->file 1->directory
	int dir_block_in_disk; //block on disk containing the last visited directory block;
	int pos_in_block; //position of the result in the visited block
	void *element; //the found element which will need to be casted according to the type of element
	DirectoryBlock *last_visited_dir_block; //if we need to add block to this directory if it's NULL we are in the dcb
} SearchResult;