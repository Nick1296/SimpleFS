#pragma once

#include "common.h"
#include "simplefs_structures.h"

//macros for gpasswd operation type
#define ADD 1
#define REMOVE 0
//macro to define and empty group entry
#define EMPTY 0

//all the operation must be executed as root user

//in memory user accounts are represented by a linked list
//this struct represents a user account
typedef struct _User {
	char account[NAME_LENGTH]; //unique username
	char password[PASS_LENGTH];
	unsigned uid; //user unique id
	unsigned gid; //id of the group associated with the user
	struct _User* next; //the next user in the list
	struct _User *prev; //the next user in the list
} User;

typedef struct _Group {
	char group_name[NAME_LENGTH];
	char password[PASS_LENGTH];
	unsigned gid; //unique group id
	unsigned group_members[GROUP_SIZE]; //list of users which are in this group
	struct _Group* next; //the next user in the list
	struct _Group *prev; // the previous user in the list
} Group;

//this is the struct which contains every useful data about users
typedef struct _Wallet{
	User* current_user; // current logged user
	User* user_list; //in memory user list
	Group* group_list; // in memory group list
	FileHandle* user_file; //file handle for /etc/passwd
	FileHandle* group_file; //gile handle for /etc/group
} Wallet;

// adds a new user
int useradd(char *username,Wallet* wallet);

// deletes a user
int userdel(char *username, Wallet *wallet);

// creates a group
int groupadd(char *name, Wallet *wallet);

//deletes a group
int groupdel(char *name, Wallet *wallet);

//adds or deletes a user from a group
int gpasswd(char *group, char *user, Wallet *wallet, int type);

//given a file handle creates a User list
User* read_users(FileHandle *users, Wallet *wallet);

//given the fs root it opens the file containing users if exists, or if not it creates it
FileHandle* load_users(DirectoryHandle* dh, Wallet *wallet);

//it saves the users array into the file
int save_users(FileHandle* users,User* data, Wallet *wallet);

//given a filehandle creates a group list
Group *read_groups(FileHandle *groups, Wallet *wallet);

//given the fs root it opens the file containing groups or if it does not exists it creates it
FileHandle *load_groups(DirectoryHandle *etc, Wallet *wallet);

//save the group list into its file
int save_groups(FileHandle *group, Group *data,Wallet *wallet);

//given a username or uid it searches the corresponding User
User* usrsrc(Wallet* wallet,char* name, unsigned uid);

//given a group name or a gid it searches the corresponding Group
Group* grpsrc(Wallet* wallet,char* name,unsigned gid);

//check if a user is in a group
int usringrp(User* usr,Group* grp);

//deletes the group list
void delete_group_list(Group* lst);

//deletes the user list
void delete_user_list(User* lst);

//given a wallet deallocate it along with its content
void destroy_wallet(Wallet* wallet);