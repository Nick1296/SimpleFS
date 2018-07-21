#pragma once

#include "simplefs.h"
#include "common.h"
#include "users.h"

//macros used to identify the permission octal values

#define READ ((uint8_t) 4)
#define WRITE ((uint8_t) 2)
#define EXECUTE ((uint8_t) 1)


/* setting the permissions on a file/directory according to the values of each set
it can be called by the root user or the owner of the file
if one permission is FAILED then the function leaves that permission unmodified
this function can takes a DirectoryHandle or a FileHandle and modifies the permissions,
you can't modify permission for a file and a directory simultaneously, so one of them MUST be NULL*/
int SimpleFS_chmod(DirectoryHandle *d, FileHandle *f, int user, int group, int others, User* current_user);

/*change a file/directory owner
it can be called by the root user or by the owner of the file
this function can takes a DirectoryHandle or a FileHandle
you can't modify ownership of a file and a directory simultaneously, so one of them MUST be NULL*/
int SimpleFS_chown(DirectoryHandle* d,FileHandle* f, unsigned new_owner, User* current_user);

// these functions will only verify that the user has enough permission before calling the correspondent SimpleFS function

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
// it requires WRITE permission on the DirectoryHandle
FileHandle *createFile(DirectoryHandle *d, const char *filename, User* current_user, Group *groups);

// reads in the (preallocated) blocks array, the name of all files in a directory
// it requires READ permission
int readDir(char **names, DirectoryHandle *d, User* current_user, Group *groups);

// opens a file in the  directory d. The file should be existing
// it requires WRITE | READ permission on the file and READ permission on the directory
FileHandle *openFile(DirectoryHandle *d, const char *filename, User* current_user, Group *groups);

// closes a file handle (destroyes it)
// it requires no permission
void closeFile(FileHandle *f);

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
// it requires WRITE permission
int writeFile(FileHandle *f, void *data, int size, User* current_user, Group *groups);

// reads in the file, at current position size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes read
// it requires READ permission
int readFile(FileHandle *f, void *data, int size, User* current_user, Group *groups);

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success -1 on error (file too short)
// it requires WRITE | READ permission
int seekFile(FileHandle *f, int pos, User* current_user, Group *groups);

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
// it requires EXECUTE permission
int changeDir(DirectoryHandle *d, const char *dirname, User* current_user, Group *groups);

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success -1 on error
// it requires WRITE permission
int mkDir(DirectoryHandle *d, const char *dirname, User* current_user, Group *groups);

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
// it requires WRITE permission
int removeFile(DirectoryHandle *d, const char *filename, User* current_user, Group *groups);