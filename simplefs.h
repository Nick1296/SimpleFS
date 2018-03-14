#pragma once
#include "bitmap.h"
#include "disk_driver.h"
#include "common.h"


/*these are structures stored on disk*/

// header, occupies the first portion of each block in the disk
// represents a chained list of blocks
typedef struct {
  int previous_block; // chained list (previous block)
  int next_block;     // chained list (next_block)
  int block_in_file; // position in the file, if 0 we have a file control block
  int block_in_disk;   // repeated position of the block on the disk
} BlockHeader;


// this is in the first block of a chain, after the header
typedef struct {
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
typedef struct {
  BlockHeader header;
  FileControlBlock fcb;
  int next_IndexBlock;
  int num_entries;
  int blocks[(BLOCK_SIZE-sizeof(FileControlBlock) - sizeof(BlockHeader)-sizeof(int)-sizeof(int))/sizeof(int)] ;
} FirstFileBlock;

typedef struct{
  int nextIndex;
	int block_in_disk;
  int previousIndex;
  int indexes[(BLOCK_SIZE-sizeof(int)-sizeof(int)-sizeof(int))/sizeof(int)];
} Index;

// this is one of the next physical blocks of a file
typedef struct {
  BlockHeader header;
  char  data[BLOCK_SIZE-sizeof(BlockHeader)];
} FileBlock;

// this is the first physical block of a directory
typedef struct {
  BlockHeader header;
  FileControlBlock fcb;
  int num_entries;
  int file_blocks[ (BLOCK_SIZE
		   -sizeof(BlockHeader)
		   -sizeof(FileControlBlock)
		    -sizeof(int))/sizeof(int) ];
} FirstDirectoryBlock;

// this is remainder block of a directory
typedef struct {
  BlockHeader header;
  int file_blocks[ (BLOCK_SIZE-sizeof(BlockHeader))/sizeof(int) ];
} DirectoryBlock;
/******************* stuff on disk END *******************/

typedef struct {
  DiskDriver* disk;
  int block_num;
  char* filename;
} SimpleFS;

// this is a file handle, used to refer to open files
typedef struct {
  SimpleFS* sfs;                   // pointer to memory file system structure
  FirstFileBlock* fcb;             // pointer to the first block of the file(read it)
  FirstDirectoryBlock* directory;  // pointer to the directory where the file is stored
  BlockHeader* current_block;      // current block in the file
  BlockHeader* current_index_block; // current block in the where the current block is saved
  int pos_in_file;                 // position of the cursor
} FileHandle;

typedef struct {
  SimpleFS* sfs;                   // pointer to memory file system structure
  FirstDirectoryBlock* dcb;        // pointer to the first block of the directory(read it)
  FirstDirectoryBlock* directory;  // pointer to the parent directory (null if top level)
  BlockHeader* current_block;      // current block in the directory
  int pos_in_dir;                  // absolute position of the cursor in the directory
  int pos_in_block;                // relative position of the cursor in the block
} DirectoryHandle;

//result of a search operation in a given directory
//the file or directory blocks, if missing are NULLs
typedef struct{
	int result; //SUCCESS or FAILED
	int type; // 0->file 1->directory
	int dir_block_in_disk; //block on disk containing the last visited directory block;
	void* element; //the found element which will need to be casted according to the type of element
	DirectoryBlock *last_visited_dir_block; //if we need to add block to this directory
}SearchResult;

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk);

// creates the inital structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
void SimpleFS_format(SimpleFS* fs);

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename);

// reads in the (preallocated) blocks array, the name of all files in a directory
int SimpleFS_readDir(char** names, DirectoryHandle* d);

// opens a file in the  directory d. The file should be exisiting
FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename);

// closes a file handle (destroyes it)
void SimpleFS_close(FileHandle* f);

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
int SimpleFS_write(FileHandle* f, void* data, int size);

// reads in the file, at current position size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes read
int SimpleFS_read(FileHandle* f, void* data, int size);

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success -1 on error (file too short)
int SimpleFS_seek(FileHandle* f, int pos);

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
 int SimpleFS_changeDir(DirectoryHandle* d, const char* dirname);

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success -1 on error
int SimpleFS_mkDir(DirectoryHandle* d, const char* dirname);

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int SimpleFS_remove(DirectoryHandle* d, const char* filename);


