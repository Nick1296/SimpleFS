#include "simplefs_shell_apis.h"
#include "simplefs_apis.h"

/* setting the permissions on a file/directory according to the values of each set
it can be called by the root user or the owner of the file
if one permission is FAILED then the function leaves that permission unmodified
this function can takes a DirectoryHandle or a FileHandle and modifies the permissions,
you can't modify permission for a file and a directory simultaneously, so one of them MUST be NULL*/
int shell_chmod(DirectoryHandle *d, char *name, int user, int group, int others, Wallet *wallet) {
	if (d == NULL || name == NULL || strlen(name) > FILENAME_MAX_LENGTH || wallet == NULL) {
		return FAILED;
	}
	int res = FAILED;
	FileHandle *fh = shell_openFile(d, name, wallet);
	if (fh != NULL) {
		res = SimpleFS_chmod(fh, user, group, others, wallet->current_user->uid);
	}
	return res;
	
}

/*change a file/directory owner
it can be called by the root user or by the owner of the file
this function can takes a DirectoryHandle or a FileHandle
you can't modify ownership of a file and a directory simultaneously, so one of them MUST be NULL*/
int shell_chown(DirectoryHandle *d, char *name, char *new_owner_name, Wallet *wallet) {
	if (d == NULL || name == NULL || strlen(name) < FILENAME_MAX_LENGTH || new_owner_name == NULL ||
	    strlen(new_owner_name) > NAME_LENGTH || wallet == NULL) {
		return FAILED;
	}
	ListElement *new_owner_lst = usrsrc(wallet, new_owner_name, MISSING);
	if (new_owner_lst == NULL || new_owner_lst->item == NULL) {
		return FAILED;
	}
	int res = FAILED;
	FileHandle *fh = shell_openFile(d, name, wallet);
	if (fh != NULL) {
		User *new_owner = new_owner_lst->item;
		res = SimpleFS_chown(fh, new_owner->uid, new_owner->gid, wallet->current_user->gid);
	}
	return res;
}

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
//in here we don't use wallet because this function it's called before the wallet can be initialized
DirectoryHandle *fs_init(SimpleFS *fs, DiskDriver *disk, int current_user, int user_primary_group) {
	if (fs == NULL || disk == NULL) {
		return NULL;
	}
	return init(fs, disk, current_user, user_primary_group);
}

// creates the initial structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the SimpleFS struct
// and set to the top level directory
//in here we don't use wallet because this function it's called before the wallet can be initialized
int shell_formatDisk(SimpleFS *fs, int current_user) {
	if (fs == NULL) {
		return NULL;
	}
	return formatDisk(fs, current_user);
}

//loads an already initialized disk
//in here we don't use wallet because this function it's called before the wallet can be initialized
int shell_loadDisk(DiskDriver *disk, const char *filename) {
	if (disk == NULL || filename == NULL) {
		return FAILED;
	}
	return loadDisk(disk, filename);
}

void shell_shutdownDisk(DiskDriver *disk) {
	shutdownDisk(disk);
}

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a block of type FirstBlock
// it requires WRITE permission on the DirectoryHandle
FileHandle *
shell_createFile(DirectoryHandle *d, const char *filename, Wallet *wallet) {
	if (d == NULL || filename == NULL || strlen(filename) > FILENAME_MAX_LENGTH || wallet == NULL) {
		return NULL;
	}
	int usr_in_grp = usringrp(wallet->current_user, NULL, d->dcb->fcb.permissions.group_uid, wallet);
	return createFile(d, filename, wallet->current_user->uid, wallet->current_user->gid, usr_in_grp);
}

// reads in the (preallocated) blocks array, the name of all files in a directory
// it requires READ permission
int shell_readDir(char **names, DirectoryHandle *d, Wallet *wallet) {
	if (names == NULL || d == NULL || wallet == NULL) {
		return FAILED;
	}
	int usr_in_grp = usringrp(wallet->current_user, NULL, d->dcb->fcb.permissions.group_uid, wallet);
	return readDir(names, d, wallet->current_user->gid, usr_in_grp);
}

// opens a file in the  directory d. The file should be existing
// it requires WRITE | READ permission on the file and READ permission on the directory
FileHandle *shell_openFile(DirectoryHandle *d, const char *filename, Wallet *wallet) {
	if (d == NULL || filename == NULL || strlen(filename) > NAME_LENGTH || wallet == NULL) {
		return NULL;
	}
	int usr_in_grp = usringrp(wallet->current_user, NULL, d->dcb->fcb.permissions.group_uid, wallet);
	return openFile(d, filename, wallet->current_user->uid, usr_in_grp);
}

// closes a file handle (destroyes it)
// it requires no permission
void shell_closeFile(FileHandle *f) {
	closeFile(f);
}

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
// it requires WRITE permission
int shell_writeFile(FileHandle *f, void *data, int size, Wallet *wallet) {
	if (f == NULL || data == NULL || size < 0 || wallet == NULL) {
		return FAILED;
	}
	int usr_in_grp = usringrp(wallet->current_user, NULL, f->fcb->fcb.permissions.group_uid, wallet);
	return writeFile(f, data, size, wallet->current_user->uid, usr_in_grp);
}

// reads in the file, at current position size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes read
// it requires READ permission
int shell_readFile(FileHandle *f, void *data, int size, Wallet *wallet) {
	if (f == NULL || data == NULL || size < 0 || wallet == NULL) {
		return FAILED;
	}
	int usr_in_grp = usringrp(wallet->current_user, NULL, f->fcb->fcb.permissions.group_uid, wallet);
	return readFile(f, data, size, wallet->current_user->uid, usr_in_grp);
}

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success -1 on error (file too short)
// it requires WRITE | READ permission
int shell_seekFile(FileHandle *f, int pos, Wallet *wallet) {
	if (f == NULL || pos < 0 || wallet == NULL) {
		return FAILED;
	}
	int usr_in_grp = usringrp(wallet->current_user, NULL, f->fcb->fcb.permissions.group_uid, wallet);
	return seekFile(f, pos, wallet->current_user->uid, usr_in_grp);
}

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
// it requires EXECUTE permission
int shell_changeDir(DirectoryHandle *d, const char *dirname, Wallet *wallet) {
	if (d == NULL || dirname == NULL || strlen(dirname) > FILENAME_MAX_LENGTH || wallet == NULL) {
		return FAILED;
	}
	int usr_in_grp = usringrp(wallet->current_user, NULL, d->dcb->fcb.permissions.group_uid, wallet);
	return changeDir(d, dirname, wallet->current_user->uid, usr_in_grp);
}

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success -1 on error
// it requires WRITE permission
int shell_mkDir(DirectoryHandle *d, const char *dirname, Wallet *wallet) {
	if (d == NULL || dirname == NULL || strlen(dirname) > FILENAME_MAX_LENGTH || wallet == NULL) {
		return FAILED;
	}
	int usr_in_grp = usringrp(wallet->current_user, NULL, d->dcb->fcb.permissions.group_uid, wallet);
	return mkDir(d, dirname, wallet->current_user->uid, wallet->current_user->gid, usr_in_grp);
}

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
// it requires WRITE permission on the directory which contains the element to remove
int shell_removeFile(DirectoryHandle *d, const char *filename, Wallet *wallet) {
	if (d == NULL || filename == NULL || strlen(filename) > FILENAME_MAX_LENGTH || wallet == NULL) {
		return FAILED;
	}
	int usr_in_grp = usringrp(wallet->current_user, NULL, d->dcb->fcb.permissions.group_uid, wallet);
	return removeFile(d, filename, wallet->current_user->uid, usr_in_grp);
}
