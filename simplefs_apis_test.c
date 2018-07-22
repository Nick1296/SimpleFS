// in this file we will test all the F function with permission enabled
#include <stdio.h>
#include "simplefs_apis.h"

void disk_info(DiskDriver *disk) {
	printf("\t Disk summary\n");
	printf("disk blocks: %d\n", disk->header->num_blocks);
	printf("disk free blocks: %d\n", disk->header->free_blocks);
	printf("disk full blocks: %d\n", disk->header->num_blocks - disk->header->free_blocks);
}

int main() {
	
	// we initialize the FS
	int res;
	DiskDriver *disk = (DiskDriver *) malloc(sizeof(DiskDriver));
	SimpleFS *fs = (SimpleFS *) malloc(sizeof(SimpleFS));
	char diskname[] = "./disk";
	unlink(diskname);
	fs->block_num = 20000;
	fs->filename = diskname;
	fs->disk = disk;
	
	format(fs, root);
	res = load(fs->disk, fs->filename);
	CHECK_ERR(res == FAILED, "can't load the fs");
	DirectoryHandle *root_dir = init(fs, disk);
	disk_info(disk);
	
	// we have tre users
	unsigned root = ROOT, usr1 = root + 1, usr2 = usr1 + 1;
	//each of these users has a group
	unsigned rootgrp[GROUP_SIZE] = {0}, usr1grp[GROUP_SIZE] = {0}, usr2grp[GROUP_SIZE] = {0};
	//in root's group there's only root
	rootgrp[0] = root;
	// in usr1's group there's usr2
	usr1grp[0] = usr1;
	usr1grp[1] = usr2;
	// in usr2's group there's only usr2
	usr2grp[0] = usr2;
	
	DirectoryHandle *usr1_dir, *usr2_dir;
	
	//we create a directory for usr1
	printf("root creates usr1 home directory\n");
	res = mkDir(root_dir, "usr1", root, rootgrp);
	CHECK_ERR(res == FAILED, "FS error on root's create usr1 directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on creating usr1 directory")
	printf("root cds into usr1 home directory\n");
	res = changeDir(root_dir, "usr1", root, rootgrp);
	CHECK_ERR(res == FAILED, "FS error on root's cd into usr1 directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on cd into usr1 directory")
	printf("root chowns usr1 dir to usr1\n");
	res = SimpleFS_chown(root_dir, NULL, usr1, root);
	
	//we copy the handle of usr1 home directory
	usr1_dir=malloc(sizeof(DirectoryHandle));
	usr1_dir->current_block=malloc(sizeof(BlockHeader));
	memcpy(usr1_dir->current_block,root_dir->current_block, sizeof(BlockHeader));
	usr1_dir->dcb=malloc(sizeof(FirstDirectoryBlock));
	memcpy(usr1_dir->dcb, root_dir->dcb, sizeof(FirstDirectoryBlock));
	usr1_dir->directory = malloc(sizeof(FirstDirectoryBlock));
	memcpy(usr1_dir->directory, root_dir->directory, sizeof(FirstDirectoryBlock));
	usr1_dir->pos_in_block=0;
	usr1_dir->pos_in_dir=0;
	usr1_dir->sfs=root_dir->sfs;
	
	//root goes back to /
	res = changeDir(root_dir, "..", root, rootgrp);
	CHECK_ERR(res == FAILED, "FS error on root's cd into .. directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on cd into .. directory")
	
	printf("root creates usr2 home directory\n");
	res = mkDir(root_dir, "usr2", root, rootgrp);
	CHECK_ERR(res == FAILED, "FS error on root's create usr2 directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on creating usr2 directory")
	printf("root cds into usr1 home directory\n");
	res = changeDir(root_dir, "usr2", root, rootgrp);
	CHECK_ERR(res == FAILED, "FS error on root's cd into usr2 directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on cd into usr2 directory")
	printf("root chowns usr2 dir to usr2\n");
	res = SimpleFS_chown(root_dir, NULL, usr2, root);
	
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
	CHECK_ERR(res == FAILED, "FS error on root's cd into .. directory")
	CHECK_ERR(res == PERM_ERR, "root perm err on cd into .. directory")
	
	//now usr1 creates a file
	FileHandle *usr1_f1=createFile(usr1_dir,"file1",usr1,usr1grp);
	CHECK_ERR(usr1_f1==NULL, "usr1 can't create a file into it's directory")
	
	//usr1 writes something in its file
	char text[1024]= {'a'},t_res[1024]={'\0'};
	text[1023]='\0';
	res= writeFile(usr1_f1,text,1024,usr1,usr1grp);
	CHECK_ERR(res == PERM_ERR,"usr1 cant write on file")
	
	//usr1 seeks to the begin of the file
	res= seekFile(usr1_f1,0,usr1,usr1grp);
	CHECK_ERR(res == PERM_ERR,"usr1 can't seek its file")
	
	//usr1 reads what has wrote and checks if it's correct
	res= readFile(usr1_f1,t_res,1024,usr1,usr1grp);
	CHECK_ERR(res==PERM_ERR,"usr1 can't delete it's file")
	
	CHECK_ERR(strcmp(text,t_res)!=0,"write/read error")
	
	//usr1 closes the file
	closeFile(usr1_f1);
	
	//now usr2 tries to create a file in the usr1 directory
	FileHandle *usr2_f2 = createFile(usr1_dir, "file2", usr2, usr2grp);
	CHECK_ERR(usr2_f2 != NULL, "usr1 can't create a file into it's directory")
	
	//now usr2 tries to open the file1 of usr1
	FileHandle *usr2_f3= openFile(usr1_dir,"file1",usr2,usr1grp);
	CHECK_ERR(usr2_f3==NULL,"usr2 can't open file1 of usr1")
	
	//usr2 writes something in this file
	memset(text,'b',1024* sizeof(char));
	text[1023]='\0';
	memset(t_res, '\0', 1024 * sizeof(char));
	res = writeFile(usr2_f3, text, 1024, usr2, usr1grp);
	CHECK_ERR(res != PERM_ERR, "usr2 can write on usr1 files")
	
	//usr2 tries to delete the file
	res = removeFile(usr1_dir, "file1", usr2, usr1grp);
	CHECK_ERR(res != PERM_ERR, "usr2 can delete usr1 files")
	
	SimpleFS_chmod(NULL, usr2_f3, READ | WRITE, READ | WRITE, READ | WRITE, root);
	
	//usr2 tries to write something in this file
	memset(text, 'b', 1024 * sizeof(char));
	text[1023] = '\0';
	memset(t_res, '\0', 1024 * sizeof(char));
	res = writeFile(usr2_f3, text, 1024, usr2, usr1grp);
	CHECK_ERR(res == PERM_ERR, "usr2 can't write on usr1 files after chmod")
	
	//usr2 seeks to the begin of the file
	res = seekFile(usr2_f3, 0, usr2, usr1grp);
	CHECK_ERR(res == PERM_ERR, "usr2 can't seek usr1 file")
	
	//usr2 reads the file
	res = readFile(usr2_f3, t_res, 1024, usr2, usr1grp);
	CHECK_ERR(res != PERM_ERR, "usr2 can't read on usr1 files")
	
	CHECK_ERR(strcmp(text, t_res) != 0, "write/read error2")
	
	//usr1 removes it's file
	res = removeFile(usr1_dir, "file1", usr1, usr1grp);
	CHECK_ERR(res == PERM_ERR, "usr1 can't delete its files")
	
	//now usr2 creates a file
	FileHandle *usr2_f1 = createFile(usr2_dir, "file1", usr2, usr2grp);
	CHECK_ERR(usr2_f1 == NULL, "usr2 can't create a file into it's directory")
	
	//usr2 closes the file
	closeFile(usr2_f1);
	
	//now usr1 tries to create a file in the usr2 directory
	FileHandle *usr1_f2 = createFile(usr2_dir, "file2", usr1, usr1grp);
	CHECK_ERR(usr1_f2 != NULL, "usr1 can create a file into usr2 directory")
	
	//now usr1 tries to open the file1 of usr2
	FileHandle *usr1_f3 = openFile(usr2_dir, "file1", usr1, usr2grp);
	CHECK_ERR(usr1_f3 == NULL, "usr1 can't open file1 of usr2")
	
	//usr1 tries to write something in this file
	memset(text, 'b', 1024 * sizeof(char));
	text[1023] = '\0';
	memset(t_res, '\0', 1024 * sizeof(char));
	res = writeFile(usr2_f1, text, 1024, usr1, usr2grp);
	CHECK_ERR(res != PERM_ERR, "usr2 can write on usr1 files")
	
	//usr1 tries to delete the file
	res = removeFile(usr2_dir, "file1", usr1, usr2grp);
	CHECK_ERR(res != PERM_ERR, "usr1 can delete usr2 files")
	
	SimpleFS_chmod(NULL,usr1_f3,READ|WRITE,READ|WRITE,READ|WRITE,root);
	
	//usr1 tries to write something in this file
	memset(text, 'b', 1024 * sizeof(char));
	text[1023] = '\0';
	memset(t_res, '\0', 1024 * sizeof(char));
	res = writeFile(usr2_f1, text, 1024, usr1, usr2grp);
	CHECK_ERR(res == PERM_ERR, "usr2 can't write on usr1 files after chmod")
	
	//usr1 seeks to the begin of the file
	res = seekFile(usr1_f3, 0, usr1, usr2grp);
	CHECK_ERR(res == PERM_ERR, "usr1 can't seek usr2 file")
	
	//usr1 reads the file
	res = readFile(usr1_f3, t_res, 1024, usr1, usr2grp);
	CHECK_ERR(res == PERM_ERR, "usr1 can't read on usr2 files")
	
	CHECK_ERR(strcmp(text, t_res) != 0, "write/read error 3")
	
	//TODO test permission on readDir, mkDir and test SimpleFS_chown on files
}