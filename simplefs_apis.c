#pragma once

#include "common.h"
#include "simplefs.h"
#include "simplefs_apis.h"
#include "users.h"
#include "disk_driver.h"


// checks the permission to authorize the function call
int check_permissions(uint8_t required, FileControlBlock fcb, User current_user, Group *groups) {
	if (fcb.permissions.user_uid == current_user.uid) {
		//check if the owner has WRITE permission on the directory
		if (fcb.permissions.user & required) {
			return SUCCESS;
		}
	} else {
		//check if the current user is in the owner's group
		if (user_in_group(fcb.permissions.group_uid, current_user.uid, groups)) {
			//checking if the group permissions include WRITE permission
			if (fcb.permissions.group & required) {
				return SUCCESS;
			}
		} else {
			//check if the other permissions include WRITE permission
			if (fcb.permissions.others & required) {
				return SUCCESS;
			}
		}
	}
	return FAILED;
}

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
// it requires WRITE permission on the DirectoryHandle
FileHandle *createFile(DirectoryHandle *d, const char *filename, User current_user, Group *groups) {
	FileHandle *f = NULL;
	int res;
	//check if the current user has permissions to perform the operation
	if (check_permissions(WRITE, d->dcb->fcb, current_user, groups) == SUCCESS) {
		f = SimpleFS_createFile(d, filename);
		//we set the owner and owner group of the newly created file
		f->fcb->fcb.permissions.user_uid = current_user.uid;
		f->fcb->fcb.permissions.group_uid = current_user.gid;
		//now we set the default permission on this file (644)
		f->fcb->fcb.permissions.user = READ | WRITE;
		f->fcb->fcb.permissions.group = READ;
		f->fcb->fcb.permissions.others = READ;
		//we write the permissions on disk
		res = DiskDriver_writeBlock(f->sfs->disk, f->fcb, f->fcb->header.block_in_disk);
		CHECK_ERR(res == FAILED, "can't update permission on the new file")
	}
	return f;
}

// reads in the (preallocated) blocks array, the name of all files in a directory
// it requires READ permission
int readDir(char **names, DirectoryHandle *d, User current_user, Group *groups) {
	int res = PERM_ERR;
	//check if the current user has permissions to perform the operation
	if (check_permissions(READ, d->dcb->fcb, current_user, groups)) {
		res = SimpleFS_readDir(names, d);
	}
	return res;
}

// opens a file in the  directory d. The file should be existing
// it requires WRITE | READ permission on the file and READ permission on the directory
FileHandle *openFile(DirectoryHandle *d, const char *filename, User current_user, Group *groups) {
	FileHandle *f = NULL;
	//check if the current user has permissions to read the directory
	if (check_permissions(READ, d->dcb->fcb, current_user, groups)) {
		f = SimpleFS_openFile(d, filename);
		//check if the current user can read or write the opened file
		if ((!check_permissions(READ, f->fcb->fcb, current_user, groups)) &&
		    (!check_permissions(WRITE, f->fcb->fcb, current_user, groups))) {
			//if not close the file
			SimpleFS_close(f);
			f = NULL;
		}
	}
	return f;
}

// closes a file handle (destroyes it)
// it requires WRITE | READ permission
void closeFile(FileHandle *f) {
	SimpleFS_close(f);
}

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
// it requires WRITE permission
int writeFile(FileHandle *f, void *data, int size, User current_user, Group *groups) {
	//checking if the user has WRITE permission on the file
	if (check_permissions(WRITE, f->fcb->fcb, current_user, groups)) {
		return SimpleFS_write(f, data, size);
	}
	return PERM_ERR;
}

// reads in the file, at current position size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes read
// it requires READ permission
int readFile(FileHandle *f, void *data, int size, User current_user, Group *groups) {
	//check if the user has READ permission on the file
	if (check_permissions(READ, f->fcb->fcb, current_user, groups)) {
		return SimpleFS_read(f, data, size);
	}
	return PERM_ERR;
}

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success -1 on error (file too short)
// it requires WRITE | READ permission
int seekFile(FileHandle *f, int pos, User current_user, Group *groups) {
	//check if the user has the read or write permissions
	if (check_permissions(READ, f->fcb->fcb, current_user, groups) ||
	    check_permissions(WRITE, f->fcb->fcb, current_user, groups)) {
		return SimpleFS_seek(f, pos);
	}
	return PERM_ERR;
}

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
// it requires EXECUTE permission
int changeDir(DirectoryHandle *d, const char *dirname, User current_user, Group *groups) {
	//checking EXECUTE permission on the directory
	if (check_permissions(EXECUTE, d->dcb->fcb, current_user, groups)) {
		return SimpleFS_changeDir(d, dirname);
	}
	return PERM_ERR;
}

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success -1 on error
// it requires WRITE permission
int mkDir(DirectoryHandle *d, const char *dirname, User current_user, Group *groups) {
	int res,res_disk;
	//we check if the user has write permission on the current directory
	if (check_permissions(WRITE, d->dcb->fcb, current_user, groups)) {
		res = SimpleFS_mkDir(d, dirname);
		//we get the DirectoryHandle of the current directory
		SimpleFS_changeDir(d, dirname);
		//we set default permission for the current user in this directory
		//we set owner and owner group
		d->dcb->fcb.permissions.user_uid = current_user.uid;
		d->dcb->fcb.permissions.group_uid = current_user.gid;
		// we set the directory default permissions (755)
		d->dcb->fcb.permissions.user = READ | WRITE | EXECUTE ;
		d->dcb->fcb.permissions.group= READ | EXECUTE;
		d->dcb->fcb.permissions.others= READ | EXECUTE;
		//we write the permission on disk
		res_disk= DiskDriver_writeBlock(d->sfs->disk,d->dcb,d->dcb->header.block_in_disk);
		CHECK_ERR(res_disk==FAILED, "can't erite directory permission on disk")
		//we go back to the parent directory
		SimpleFS_changeDir(d,"..");
		return res;
	}
	return PERM_ERR;
}

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
// it requires WRITE permission on the directory which contains the element to remove
int removeFile(DirectoryHandle *d, const char *filename, User current_user, Group *groups) {
	//we check if the user has write permission on the current directory
	if (check_permissions(WRITE, d->dcb->fcb, current_user, groups)) {
		return SimpleFS_remove(d,filename);
	}
	return PERM_ERR;
}