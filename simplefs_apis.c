
#include "simplefs.h"
#include "simplefs_apis.h"
#include <stdio.h>

// checks the permission to authorize the function call
//root user has access to EVERYTHING
int check_permissions(uint8_t required, FileControlBlock fcb, unsigned current_user, int usr_in_grp) {
	if (fcb.permissions.user_uid == current_user || current_user==ROOT) {
		//check if the owner has requested permission on the directory
		if (fcb.permissions.user & required || current_user==ROOT) {
			return SUCCESS;
		}
	} else {
		//check if the current user is in the owner's group
		if (usr_in_grp==SUCCESS) {
			//checking if the group permissions include requested permission
			if (fcb.permissions.group & required) {
				return SUCCESS;
			}
		} else {
			//check if the other permissions include requested permission
			if (fcb.permissions.others & required) {
				return SUCCESS;
			}
		}
	}
	return FAILED;
}


// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle *init(SimpleFS *fs, DiskDriver *disk,unsigned  current_user){
	DirectoryHandle *dh;
	if(current_user==ROOT){
	dh = SimpleFS_init(fs,disk);
	//we set the default permissions (755)
	dh->dcb->fcb.permissions.user_uid=current_user;
	dh->dcb->fcb.permissions.group_uid=current_user;
	dh->dcb->fcb.permissions.user= READ | WRITE | EXECUTE;
	dh->dcb->fcb.permissions.group= READ|EXECUTE;
	dh->dcb->fcb.permissions.others= READ|EXECUTE;
	//saving the updated Directory
	DiskDriver_writeBlock(disk,dh->dcb, sizeof(FirstDirectoryBlock));
	return dh;
	}
	return NULL;
}

// creates the initial structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
int format(SimpleFS *fs, unsigned current_user){
	if(current_user==ROOT){
		SimpleFS_format(fs);
		return SUCCESS;
	}
	return PERM_ERR;
}

//loads an already initialized disk
int load(DiskDriver *disk, const char *filename){
	return DiskDriver_load(disk,filename);
}

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
// it requires WRITE permission on the DirectoryHandle
FileHandle *createFile(DirectoryHandle *d, const char *filename, unsigned current_user, int usr_in_grp) {
	FileHandle *f = NULL;
	int res;
	//check if the current user has permissions to perform the operation
	if (check_permissions(WRITE, d->dcb->fcb, current_user, usr_in_grp) == SUCCESS) {
		f = SimpleFS_createFile(d, filename);
		//we set the owner and owner group of the newly created file
		f->fcb->fcb.permissions.user_uid = current_user;
		f->fcb->fcb.permissions.group_uid = current_user;
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
int readDir(char **names, DirectoryHandle *d, unsigned current_user, int usr_in_grp) {
	int res = PERM_ERR;
	//check if the current user has permissions to perform the operation
	if (check_permissions(READ, d->dcb->fcb, current_user, usr_in_grp)==SUCCESS) {
		res = SimpleFS_readDir(names, d);
	}
	return res;
}

// opens a file in the  directory d. The file should be existing
// it requires WRITE | READ permission on the file and READ permission on the directory
FileHandle *openFile(DirectoryHandle *d, const char *filename, unsigned current_user, int usr_in_grp) {
	FileHandle *f = NULL;
	//check if the current user has permissions to read the directory
	if (check_permissions(READ, d->dcb->fcb, current_user, usr_in_grp)==SUCCESS) {
		f = SimpleFS_openFile(d, filename);
		if(f!=NULL) {
			//check if the current user can read or write the opened file
			if (check_permissions(READ, f->fcb->fcb, current_user, usr_in_grp) != SUCCESS &&
			    check_permissions(WRITE, f->fcb->fcb, current_user, usr_in_grp) != SUCCESS) {
				//if not close the file
				SimpleFS_close(f);
				f = NULL;
			}
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
int writeFile(FileHandle *f, void *data, int size, unsigned current_user, int usr_in_grp) {
	//checking if the user has WRITE permission on the file
	if (check_permissions(WRITE, f->fcb->fcb, current_user, usr_in_grp)==SUCCESS) {
		return SimpleFS_write(f, data, size);
	}
	return PERM_ERR;
}

// reads in the file, at current position size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes read
// it requires READ permission
int readFile(FileHandle *f, void *data, int size, unsigned current_user, int usr_in_grp) {
	//check if the user has READ permission on the file
	if (check_permissions(READ, f->fcb->fcb, current_user, usr_in_grp)==SUCCESS) {
		return SimpleFS_read(f, data, size);
	}
	return PERM_ERR;
}

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success -1 on error (file too short)
// it requires WRITE | READ permission
int seekFile(FileHandle *f, int pos, unsigned current_user, int usr_in_grp) {
	//check if the user has the read or write permissions
	if (check_permissions(READ, f->fcb->fcb, current_user, usr_in_grp)==SUCCESS ||
	    check_permissions(WRITE, f->fcb->fcb, current_user, usr_in_grp)==SUCCESS) {
		return SimpleFS_seek(f, pos);
	}
	return PERM_ERR;
}

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
// it requires EXECUTE permission
int changeDir(DirectoryHandle *d, const char *dirname, unsigned current_user, int usr_in_grp) {
	//checking EXECUTE permission on the directory
	if (check_permissions(EXECUTE, d->dcb->fcb, current_user, usr_in_grp)==SUCCESS) {
		return SimpleFS_changeDir(d, dirname);
	}
	return PERM_ERR;
}

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success -1 on error
// it requires WRITE permission
int mkDir(DirectoryHandle *d, const char *dirname, unsigned current_user, int usr_in_grp) {
	int res, res_disk;
	//we check if the user has write permission on the current directory
	if (check_permissions(WRITE, d->dcb->fcb, current_user, usr_in_grp)==SUCCESS) {
		res = SimpleFS_mkDir(d, dirname);
		//we get the DirectoryHandle of the current directory
		SimpleFS_changeDir(d, dirname);
		//we set default permission for the current user in this directory
		//we set owner and owner group
		d->dcb->fcb.permissions.user_uid = current_user;
		d->dcb->fcb.permissions.group_uid = current_user;
		// we set the directory default permissions (755)
		d->dcb->fcb.permissions.user = READ | WRITE | EXECUTE;
		d->dcb->fcb.permissions.group = READ | EXECUTE;
		d->dcb->fcb.permissions.others = READ | EXECUTE;
		//we write the permission on disk
		res_disk = DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->header.block_in_disk);
		CHECK_ERR(res_disk == FAILED, "can't write directory permission on disk")
		//we go back to the parent directory
		SimpleFS_changeDir(d, "..");
		return res;
	}
	return PERM_ERR;
}

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
// it requires WRITE permission on the directory which contains the element to remove
int removeFile(DirectoryHandle *d, const char *filename, unsigned current_user, int usr_in_grp) {
	//we check if the user has write permission on the current directory
	if (check_permissions(WRITE, d->dcb->fcb, current_user, usr_in_grp)==SUCCESS) {
		return SimpleFS_remove(d, filename);
	}
	return PERM_ERR;
}

/* setting the permissions on a file/directory according to the values of each set
it can be called by the root user or the owner of the file
if one permission is FAILED then the function leaves that permission unmodified
this function can takes a DirectoryHandle or a FileHandle and modifies the permissions,
you cant modify permission for a file and a directory simultaneously, so one of them MUST be NULL*/
int SimpleFS_chmod(DirectoryHandle* d,FileHandle* f, int user, int group, int others, unsigned current_user){
	//chmod can only be execute by root user
	if(current_user!=ROOT){
		return PERM_ERR;
	}
	int res;
	//we check if we are operating on a file o directory
	if(f!=NULL){
		//now we change permissions only for the specified fields
		if (user != FAILED) {
			f->fcb->fcb.permissions.user = (uint8_t) user;
		}
		if (group != FAILED) {
			f->fcb->fcb.permissions.group = (uint8_t) group;
		}
		if (others != FAILED) {
			f->fcb->fcb.permissions.others = (uint8_t) others;
		}
		// we write the file with the new permissions on disk
		res = DiskDriver_writeBlock(f->sfs->disk, f->fcb, f->fcb->header.block_in_disk);
		CHECK_ERR(res == FAILED, "can't save new file permissions")
		return SUCCESS;
	}
	
	if (d != NULL) {
		//now we change permissions only for the specified fields
		if (user != FAILED) {
			d->dcb->fcb.permissions.user= (uint8_t) user;
		}
		if (group != FAILED) {
			d->dcb->fcb.permissions.group = (uint8_t) group;
		}
		if (others != FAILED) {
			d->dcb->fcb.permissions.others = (uint8_t) others;
		}
		// we write the file with the new permissions on disk
		res = DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->header.block_in_disk);
		CHECK_ERR(res == FAILED, "can't save new directory permissions")
		return SUCCESS;
	}
	return FAILED;
}

/*change a file/directory owner
it can be called by the root user or by the owner of the file
this function can takes a DirectoryHandle or a FileHandle
you can't modify ownership of a file and a directory simultaneously, so one of them MUST be NULL*/
int SimpleFS_chown(DirectoryHandle *d, FileHandle *f, unsigned new_owner, unsigned current_user){
// if we have a file we change its ownership
	int res;
	if(f!=NULL){
		// we check that we are authorized to change the ownership
		if(f->fcb->fcb.permissions.user_uid!=current_user && current_user!=ROOT){
			return PERM_ERR;
		}
		//we change the ownership
		f->fcb->fcb.permissions.user_uid=new_owner;
		f->fcb->fcb.permissions.group_uid= new_owner;
		// we save the changes on disk
		res=DiskDriver_writeBlock(f->sfs->disk,f->fcb,f->fcb->header.block_in_disk);
		CHECK_ERR(res == FAILED, "can't save new ownership of the file")
		return SUCCESS;
	}
	
	if (d != NULL) {
		// we check that we are authorized to change the ownership
		if (d->dcb->fcb.permissions.user_uid != current_user && current_user != ROOT) {
			return PERM_ERR;
		}
		//we change the ownership
		d->dcb->fcb.permissions.user_uid = new_owner;
		d->dcb->fcb.permissions.group_uid = new_owner;
		// we save the changes on disk
		res = DiskDriver_writeBlock(d->sfs->disk, d->dcb, d->dcb->header.block_in_disk);
		CHECK_ERR(res == FAILED, "can't save new ownership of the file")
		return SUCCESS;
	}
	return FAILED;
}