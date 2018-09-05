// in this file we will test all the F function with permission enabled
#include <stdio.h>
#include "simplefs_apis.h"

void disk_info(DiskDriver *disk) {
	printf("\t Disk summary\n");
	printf("disk blocks: %d\n", disk->header->num_blocks);
	printf("disk free blocks: %d\n", disk->header->free_blocks);
	printf("disk full blocks: %d\n", disk->header->num_blocks - disk->header->free_blocks);
}

int main(void) {
	
	// we initialize the FS
	int res;
	DiskDriver *disk = (DiskDriver *) malloc(sizeof(DiskDriver));
	SimpleFS *fs = (SimpleFS *) malloc(sizeof(SimpleFS));
	char diskname[] = "./disk";
	unlink(diskname);
	fs->block_num = 20000;
	fs->filename = diskname;
	fs->disk = disk;
	
	unsigned root = ROOT;
	format(fs, root);
	res = load(fs->disk, fs->filename);
	CHECK_ERR(res == FAILED, "can't load the fs");
	DirectoryHandle *root_dir = init(fs, disk, root);
	disk_info(disk);
	
	// we have two users
	unsigned usr1 = root + 1, usr2 = usr1 + 1;
	//each of these users has a group
	int rootgrp, usr1grp, usr2grp;
	//in root's group there's only root
	rootgrp = FAILED;
	// in usr1's group there's usr2
	usr1grp = SUCCESS;
	// in usr2's group there's only usr2
	usr2grp = FAILED;
	
	DirectoryHandle *usr1_dir, *usr2_dir;
	
	//we create a directory for usr1
	printf("root creates usr1 home directory\n");
	res = mkDir(root_dir, "usr1", root, rootgrp);
	printf("result %d\n", res);
	CHECK_ERR(res == FAILED, "FS error on root's create usr1 directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on creating usr1 directory")
	printf("root cds into usr1 home directory\n");
	res = changeDir(root_dir, "usr1", root, rootgrp);
	printf("result %d\n", res);
	CHECK_ERR(res == FAILED, "FS error on root's cd into usr1 directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on cd into usr1 directory")
	printf("root chowns usr1 dir to usr1\n");
	res = SimpleFS_chown(root_dir, NULL, usr1, root);
	printf("result %d\n", res);
	//we copy the handle of usr1 home directory
	usr1_dir = malloc(sizeof(DirectoryHandle));
	usr1_dir->current_block = malloc(sizeof(BlockHeader));
	memcpy(usr1_dir->current_block, root_dir->current_block, sizeof(BlockHeader));
	usr1_dir->dcb = malloc(sizeof(FirstDirectoryBlock));
	memcpy(usr1_dir->dcb, root_dir->dcb, sizeof(FirstDirectoryBlock));
	usr1_dir->directory = malloc(sizeof(FirstDirectoryBlock));
	memcpy(usr1_dir->directory, root_dir->directory, sizeof(FirstDirectoryBlock));
	usr1_dir->pos_in_block = 0;
	usr1_dir->pos_in_dir = 0;
	usr1_dir->sfs = root_dir->sfs;
	
	//root goes back to /
	res = changeDir(root_dir, "..", root, rootgrp);
	printf("result %d\n", res);
	CHECK_ERR(res == FAILED, "FS error on root's cd into .. directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on cd into .. directory")
	
	printf("root creates usr2 home directory\n");
	res = mkDir(root_dir, "usr2", root, rootgrp);
	printf("result %d\n", res);
	CHECK_ERR(res == FAILED, "FS error on root's create usr2 directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on creating usr2 directory")
	printf("root cds into usr1 home directory\n");
	res = changeDir(root_dir, "usr2", root, rootgrp);
	printf("result %d\n", res);
	CHECK_ERR(res == FAILED, "FS error on root's cd into usr2 directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on cd into usr2 directory")
	printf("root chowns usr2 dir to usr2\n");
	res = SimpleFS_chown(root_dir, NULL, usr2, root);
	printf("result %d\n", res);
	
	//we copy the handle of usr2 home directory
	usr2_dir = malloc(sizeof(DirectoryHandle));
	usr2_dir->current_block = malloc(sizeof(BlockHeader));
	memcpy(usr2_dir->current_block, root_dir->current_block, sizeof(BlockHeader));
	usr2_dir->dcb = malloc(sizeof(FirstDirectoryBlock));
	memcpy(usr2_dir->dcb, root_dir->dcb, sizeof(FirstDirectoryBlock));
	usr2_dir->directory = malloc(sizeof(FirstDirectoryBlock));
	memcpy(usr2_dir->directory, root_dir->directory, sizeof(FirstDirectoryBlock));
	usr2_dir->pos_in_block = 0;
	usr2_dir->pos_in_dir = 0;
	usr2_dir->sfs = root_dir->sfs;
	
	//root goes back to /
	res = changeDir(root_dir, "..", root, rootgrp);
	printf("result %d\n", res);
	CHECK_ERR(res == FAILED, "FS error on root's cd into .. directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on cd into .. directory")
	
	//now usr1 creates a file
	printf("usr1 creates a file\n");
	FileHandle *usr1_f1 = createFile(usr1_dir, "file1", usr1, usr1grp);
	CHECK_ERR(usr1_f1 == NULL, "usr1 can't create a file into it's directory")
	
	//usr1 writes something in its file
	printf("usr1 writes in its file\n");
	char text[1024] = {'a'}, t_res[1024] = {'\0'};
	text[1023] = '\0';
	res = writeFile(usr1_f1, text, 1024, usr1, usr1grp);
	printf("result %d\n", res);
	CHECK_ERR(res == PERM_ERR, "usr1 cant write on file")
	
	//usr1 seeks to the begin of the file
	printf("usr1 seek at the begin of the file\n");
	res = seekFile(usr1_f1, 0, usr1, usr1grp);
	printf("result %d\n", res);
	CHECK_ERR(res == PERM_ERR, "usr1 can't seek its file")
	
	//usr1 reads what has wrote and checks if it's correct
	printf("usr1 reads its file\n");
	res = readFile(usr1_f1, t_res, 1024, usr1, usr1grp);
	printf("result %d\n", res);
	CHECK_ERR(res == PERM_ERR, "usr1 can't delete it's file")
	
	CHECK_ERR(strcmp(text, t_res) != 0, "write/read error")
	
	//usr1 closes the file
	closeFile(usr1_f1);
	
	//now usr2 tries to create a file in the usr1 directory
	printf("usr2 creates a file in usr1 directory\n");
	FileHandle *usr2_f2 = createFile(usr1_dir, "file2", usr2, usr2grp);
	CHECK_ERR(usr2_f2 != NULL, "usr2 can create a file into it's directory")
	
	//now usr2 tries to open the file1 of usr1
	printf("usr2 opens usr1's file\n");
	FileHandle *usr2_f3 = openFile(usr1_dir, "file1", usr2, usr1grp);
	CHECK_ERR(usr2_f3 == NULL, "usr2 can't open file1 of usr1")
	
	//usr2 writes something in this file
	memset(text, 'b', 1024 * sizeof(char));
	text[1023] = '\0';
	memset(t_res, '\0', 1024 * sizeof(char));
	res = writeFile(usr2_f3, text, 1024, usr2, usr1grp);
	CHECK_ERR(res != PERM_ERR, "usr2 can write on usr1 files")
	
	//usr2 tries to delete the file
	printf("usr2 tries to remove user1's file\n");
	res = removeFile(usr1_dir, "file1", usr2, usr1grp);
	printf("result %d\n", res);
	CHECK_ERR(res != PERM_ERR, "usr2 can delete usr1 files")
	
	printf("root grants all permission to everyone on usr1 file\n");
	
	res = SimpleFS_chmod(NULL, usr2_f3, READ | WRITE, READ | WRITE, READ | WRITE, root);
	printf("result %d\n", res);
	
	//usr2 seeks to the begin of the file
	printf("usr2 seeks to the begin of usr1 file\n");
	res = seekFile(usr2_f3, 0, usr2, usr1grp);
	printf("result %d\n", res);
	CHECK_ERR(res == PERM_ERR, "usr2 can't seek usr1 file")
	
	//usr2 tries to write something in this file
	printf("usr2 tries to write something into usr1 file\n");
	memset(text, 'b', 1024 * sizeof(char));
	text[1023] = '\0';
	memset(t_res, '\0', 1024 * sizeof(char));
	res = writeFile(usr2_f3, text, 1024, usr2, usr1grp);
	printf("result %d\n", res);
	CHECK_ERR(res == PERM_ERR, "usr2 can't write on usr1 files after chmod")
	
	//usr2 seeks to the begin of the file
	printf("usr2 seeks to the begin of usr1 file\n");
	res = seekFile(usr2_f3, 0, usr2, usr1grp);
	printf("result %d\n", res);
	CHECK_ERR(res == PERM_ERR, "usr2 can't seek usr1 file")
	
	//usr2 reads the file
	printf("usr2 checks what has wrote\n");
	res = readFile(usr2_f3, t_res, 1024, usr2, usr1grp);
	printf("result %d\n", res);
	CHECK_ERR(res == PERM_ERR, "usr2 can't read on usr1 files")
	
	CHECK_ERR(strcmp(text, t_res) != 0, "write/read error2")
	
	//usr1 removes it's file
	printf("usr1 deletes its file\n");
	res = removeFile(usr1_dir, "file1", usr1, usr1grp);
	printf("result %d\n", res);
	CHECK_ERR(res == PERM_ERR, "usr1 can't delete its files")
	
	//now usr2 creates a file
	printf("usr2 creates a file int's directory\n");
	FileHandle *usr2_f1 = createFile(usr2_dir, "file1", usr2, usr2grp);
	CHECK_ERR(usr2_f1 == NULL, "usr2 can't create a file into it's directory")
	
	//usr2 closes the file
	closeFile(usr2_f1);
	
	//now usr1 tries to create a file in the usr2 directory
	printf("usr1 tries to create a file in usr2 directory\n");
	FileHandle *usr1_f2 = createFile(usr2_dir, "file2", usr1, usr1grp);
	CHECK_ERR(usr1_f2 != NULL, "usr1 can create a file into usr2 directory")
	
	//now usr1 tries to open the file1 of usr2
	printf("usr1 tries to open the file of usr2\n");
	FileHandle *usr1_f3 = openFile(usr2_dir, "file1", usr1, usr2grp);
	CHECK_ERR(usr1_f3 == NULL, "usr1 can't open file1 of usr2")
	
	//usr1 tries to write something in this file
	printf("usr1 tries to write something into usr2 file\n");
	memset(text, 'b', 1024 * sizeof(char));
	text[1023] = '\0';
	memset(t_res, '\0', 1024 * sizeof(char));
	res = writeFile(usr1_f3, text, 1024, usr1, usr2grp);
	printf("result %d\n", res);
	CHECK_ERR(res != PERM_ERR, "usr2 can write on usr1 files")
	
	//usr1 tries to delete the file
	printf("usr1 tries to remove usr2's file");
	res = removeFile(usr2_dir, "file1", usr1, usr2grp);
	printf("result %d\n", res);
	CHECK_ERR(res != PERM_ERR, "usr1 can delete usr2 files")
	
	printf("root gives all permission to every one on usr2 file\n");
	SimpleFS_chmod(NULL, usr1_f3, READ | WRITE, READ | WRITE, READ | WRITE, root);
	
	//usr1 tries to write something in this file
	printf("usr1 tries to write something into usr2 after having the permission granted");
	memset(text, 'b', 1024 * sizeof(char));
	text[1023] = '\0';
	memset(t_res, '\0', 1024 * sizeof(char));
	res = writeFile(usr1_f3, text, 1024, usr1, usr2grp);
	CHECK_ERR(res == PERM_ERR, "usr2 can't write on usr1 files after chmod")
	
	//usr1 seeks to the begin of the file
	printf("usr1 seeks at the begin of the file\n");
	res = seekFile(usr1_f3, 0, usr1, usr2grp);
	printf("result %d\n", res);
	CHECK_ERR(res == PERM_ERR, "usr1 can't seek usr2 file")
	
	//usr1 reads the file
	printf("usr1 check if what has wrote it's correct\n");
	res = readFile(usr1_f3, t_res, 1024, usr1, usr2grp);
	CHECK_ERR(res == PERM_ERR, "usr1 can't read on usr2 files")
	
	CHECK_ERR(strcmp(text, t_res) != 0, "write/read error 3")
	
	printf("root sets default perms on usr2 file\n");
	res = SimpleFS_chmod(NULL, usr1_f3, READ | WRITE, READ, READ, root);
	printf("result %d\n", res);
	
	printf("usr2 gives ownership to usr1\n");
	res = SimpleFS_chown(NULL, usr1_f3, usr1, usr2);
	printf("result %d\n", res);
	
	printf("usr2 tries to write in the new owned file\n");
	memset(text, 'c', 1024 * sizeof(char));
	text[1023] = '\0';
	memset(t_res, '\0', 1024 * sizeof(char));
	res = writeFile(usr1_f3, text, 1024, usr2, usr1grp);
	CHECK_ERR(res != PERM_ERR, "usr2 can write on new owned file after chown")
	
	//now usr1 creates a directory in its directory
	printf("usr1 creates a folder in it's directory\n");
	res = mkDir(usr1_dir, "dir_test", usr1, usr1grp);
	printf("result: %d\n", res);
	CHECK_ERR(res == FAILED, "FS error in creating a directory")
	CHECK_ERR(res == PERM_ERR, "usr1 can't create a directory in it's directory")
	
	//now usr1 creates a directory in usr2 directory
	printf("usr1 tries to create a folder in usr2 directory\n");
	res = mkDir(usr2_dir, "dir_test2", usr1, usr2grp);
	printf("result: %d\n", res);
	CHECK_ERR(res == FAILED, "FS error in creating a directory")
	CHECK_ERR(res != PERM_ERR, "usr1 can create a directory in usr2 directory")
	
	printf("root gives permission to group to modify usr2 directory\n");
	res = SimpleFS_chmod(usr2_dir, NULL, FAILED, READ | WRITE, FAILED, root);
	printf("result %d\n", res);
	
	//now usr1 creates a directory in usr2 directory
	printf("usr1 tries to create a folder in usr2 directory after having given permission\n");
	res = mkDir(usr1_dir, "dir_test2", usr1, usr2grp);
	printf("result: %d\n", res);
	CHECK_ERR(res == FAILED, "FS error in creating a directory")
	CHECK_ERR(res == PERM_ERR, "usr1 can't create a directory in usr2 directory after given permission")
	
	//now usr1 creates a directory in usr2 directory
	printf("usr1 tries to read usr2 directory\n");
	char *files;
	res = readDir(&files, usr2_dir, usr1, usr2grp);
	printf("result: %d\n", res);
	CHECK_ERR(res == FAILED, "FS error in reading a directory")
	CHECK_ERR(res == PERM_ERR, "usr1 read usr2 directory")
}