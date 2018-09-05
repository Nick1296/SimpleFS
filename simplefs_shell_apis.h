#pragma once

//this file contain the APIs function that can be called by the shell
//these functions retrieve the current user and check if the current user is in the resource group before calling the equivalent from simplefs_apis
#include "simplefs_structures.h"
#include "common.h"
#include "disk_driver_structures.h"
#include "users.h"


/* setting the permissions on a file/directory according to the values of each set
it can be called by the root user or the owner of the file
if one permission is FAILED then the function leaves that permission unmodified
this function can takes a DirectoryHandle or a FileHandle and modifies the permissions,
you can't modify permission for a file and a directory simultaneously, so one of them MUST be NULL*/
int shell_chmod(DirectoryHandle *d, char *name, int user, int group, int others, Wallet *wallet);

/*change a file/directory owner
it can be called by the root user or by the owner of the file
this function can takes a DirectoryHandle or a FileHandle
you can't modify ownership of a file and a directory simultaneously, so one of them MUST be NULL*/
int shell_chown(DirectoryHandle *d, char *name, char *new_owner, Wallet *wallet);

// these functions will only verify that the user has enough permission before calling the correspondent SimpleFS function

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle *fs_init(SimpleFS *fs, DiskDriver *disk, int current_user, int user_primary_group);

// creates the initial structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
int shell_formatDisk(SimpleFS *fs, int current_user);

//loads an already initialized disk
int shell_loadDisk(DiskDriver *disk, const char *filename);

void shell_shutdownDisk(DiskDriver *disk);

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
// it requires WRITE permission on the DirectoryHandle
FileHandle *
shell_createFile(DirectoryHandle *d, const char *filename, Wallet *wallet);

// reads in the (preallocated) blocks array, the name of all files in a directory
// it requires READ permission
int shell_readDir(char **names, DirectoryHandle *d, Wallet *wallet);

// opens a file in the  directory d. The file should be existing
// it requires WRITE | READ permission on the file and READ permission on the directory
FileHandle *shell_openFile(DirectoryHandle *d, const char *filename, Wallet *wallet);

// closes a file handle (destroyes it)
// it requires no permission
void shell_closeFile(FileHandle *f);

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
// it requires WRITE permission
int shell_writeFile(FileHandle *f, void *data, int size, Wallet *wallet);

// reads in the file, at current position size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes read
// it requires READ permission
int shell_readFile(FileHandle *f, void *data, int size, Wallet *wallet);

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success -1 on error (file too short)
// it requires WRITE | READ permission
int shell_seekFile(FileHandle *f, int pos, Wallet *wallet);

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
// it requires EXECUTE permission
int shell_changeDir(DirectoryHandle *d, const char *dirname, Wallet *wallet);

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success -1 on error
// it requires WRITE permission
int shell_mkDir(DirectoryHandle *d, const char *dirname, Wallet *wallet);

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
// it requires WRITE permission on the directory which contains the element to remove
int shell_removeFile(DirectoryHandle *d, const char *filename, Wallet *wallet);