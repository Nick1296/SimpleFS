#pragma once
//macros used to identify the permission octal values
#include "simplefs_structures.h"
#include "common.h"
#include "disk_driver_structures.h"

#define READ 4
#define WRITE 2
#define EXECUTE 1


/* setting the permissions on a file/directory according to the values of each set
it can be called by the root user or the owner of the file
if one permission is FAILED then the function leaves that permission unmodified
this function can takes a DirectoryHandle or a FileHandle and modifies the permissions,
you can't modify permission for a file and a directory simultaneously, so one of them MUST be NULL*/
int SimpleFS_chmod(DirectoryHandle *d, FileHandle *f, int user, int group, int others, unsigned current_user);

/*change a file/directory owner
it can be called by the root user or by the owner of the file
this function can takes a DirectoryHandle or a FileHandle
you can't modify ownership of a file and a directory simultaneously, so one of them MUST be NULL*/
int SimpleFS_chown(DirectoryHandle* d,FileHandle* f, unsigned new_owner, unsigned current_user);

// these functions will only verify that the user has enough permission before calling the correspondent SimpleFS function

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle *init(SimpleFS *fs, DiskDriver *disk, unsigned current_user);

// creates the initial structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
int format(SimpleFS *fs, unsigned current_user);

//loads an already initialized disk
int load(DiskDriver *disk, const char *filename);

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
// it requires WRITE permission on the DirectoryHandle
FileHandle *createFile(DirectoryHandle *d, const char *filename, unsigned current_user, int usr_in_grp);

// reads in the (preallocated) blocks array, the name of all files in a directory
// it requires READ permission
int readDir(char **names, DirectoryHandle *d, unsigned current_user, int usr_in_grp);

// opens a file in the  directory d. The file should be existing
// it requires WRITE | READ permission on the file and READ permission on the directory
FileHandle *openFile(DirectoryHandle *d, const char *filename, unsigned current_user, int usr_in_grp);

// closes a file handle (destroyes it)
// it requires no permission
void closeFile(FileHandle *f);

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
// it requires WRITE permission
int writeFile(FileHandle *f, void *data, int size, unsigned current_user, int usr_in_grp);

// reads in the file, at current position size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes read
// it requires READ permission
int readFile(FileHandle *f, void *data, int size, unsigned current_user, int usr_in_grp);

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success -1 on error (file too short)
// it requires WRITE | READ permission
int seekFile(FileHandle *f, int pos, unsigned current_user, int usr_in_grp);

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
// it requires EXECUTE permission
int changeDir(DirectoryHandle *d, const char *dirname, unsigned current_user, int usr_in_grp);

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success -1 on error
// it requires WRITE permission
int mkDir(DirectoryHandle *d, const char *dirname, unsigned current_user, int usr_in_grp);

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
// it requires WRITE permission on the directory which contains the element to remove
int removeFile(DirectoryHandle *d, const char *filename, unsigned current_user, int usr_in_grp);