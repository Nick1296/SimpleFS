#pragma once

#include "common.h"

//this struct represents a user account
typedef struct _User {
	char account[NAME_LENGTH]; //unique username
	char password[PASS_LENGTH];
	unsigned uid; //user unique id
	unsigned gid; //id of the group associated with the user
} User;

typedef struct _Group {
	char group_name[NAME_LENGTH];
	char password[PASS_LENGTH];
	unsigned gid; //unique group id
	unsigned group_members[GROUP_SIZE]; //list of users which are in this group
} Group;

// adds a new user

User *useradd(char *username, FileHandle *users);

// deletes a user
int userdel(char *username, FileHandle *users);

// creates a group
Group *groupadd(char *name, FileHandle *groups);

//deletes a group
int groupdel(char *name, FileHandle *groups);

//adds or deletes a user from a group
int gpasswd(char *group, char *user, FileHandle *groups, int type);