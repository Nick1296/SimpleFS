#pragma once

#include "common.h"
#include "simplefs_structures.h"
#include "list.h"

//macros for gpasswd operation type
#define ADD 1
#define REMOVE 0
//macro to define and empty group entry
#define EMPTY (-1)

//all the operation must be executed as root user

//this struct represents a user account
typedef struct _User {
	char account[NAME_LENGTH]; //unique username
	int uid; //user unique id
	int gid; //id of the group associated with the user
} User;

//this is a group account
typedef struct _Group {
	char group_name[NAME_LENGTH];
	int gid; //unique group id
	int group_members[GROUP_SIZE]; //list of users which are in this group
} Group;

//struct which contains the last uid and gid generated
typedef struct _Ids {
	int last_uid; //last uid generated
	int last_gid; //last gid generated
} Ids;
//this is the struct which contains every useful data about users
typedef struct _Wallet {
	User *current_user; // current logged user
	ListHead *user_list; //in memory user list
	ListHead *group_list; // in memory group list
	FileHandle *user_file; //file handle for /etc/passwd
	FileHandle *group_file; //file handle for /etc/group
	FileHandle *ids_file; //file handle for /etc/ids which stores on disk the last uid and gid generated
	Ids *ids; //last uid and gid generated
} Wallet;

// adds a new user
int useradd(char *username, DirectoryHandle *dh, Wallet *wallet);

// deletes a user
int userdel(char *username, DirectoryHandle *dh, Wallet *wallet);

// creates a group
int groupadd(char *name, Wallet *wallet);

//deletes a group
int groupdel(char *name, Wallet *wallet);

//adds or deletes a user from a group
int gpasswd(char *group, char *user, Wallet *wallet, int type);

//given a file handle creates a User list
ListHead *read_users(FileHandle *users, Wallet *wallet);

//given the fs root it opens the file containing users if exists, or if not it creates it
FileHandle *load_users(DirectoryHandle *dh, Wallet *wallet);

//it saves the users array into the file
int save_users(FileHandle *users, ListHead *data, Wallet *wallet);

//given a filehandle creates a group list
ListHead *read_groups(FileHandle *groups, Wallet *wallet);

//given the fs root it opens the file containing groups or if it does not exists it creates it
FileHandle *load_groups(DirectoryHandle *etc, Wallet *wallet);

//save the group list into its file
int save_groups(FileHandle *group, ListHead *data, Wallet *wallet);

//given a username or uid it searches the corresponding User
ListElement *usrsrc(Wallet *wallet, char *name, int uid);

//given a group name or a gid it searches the corresponding Group
ListElement *grpsrc(Wallet *wallet, char *name, int gid);

//check if a user is in a group
int usringrp(User *usr, char *grp_name, int gid, Wallet *wallet);

//given a wallet deallocate it along with its content
void destroy_wallet(Wallet *wallet);

//load the user subsytem and if there are no users in the system creates root user
//the directory handle given must be the root of the disk
Wallet *initialize_wallet(DirectoryHandle *dh);

//opens or create the file which stores the last uid and gid created by the fs
//return NULL on error
FileHandle *load_ids(DirectoryHandle *dh, Wallet *wallet);

//given the handle to the id file we load its content on memory
//returns NULL on error
Ids *read_ids(FileHandle *id, Wallet *wallet);

//given the handle to the id file we save on it the ids in the wallet
int save_ids(FileHandle *id, Wallet *wallet);