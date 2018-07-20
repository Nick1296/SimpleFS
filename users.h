#pragma once

#include "simplefs.h"
//this struct represents a user account
typedef struct _User {
	char account[128]; //unique username
	char password[128];
	unsigned uid; //user unique id
	unsigned gid; //id of the group associated with the user
} User;

typedef struct _Group {
	char group_name[128];
	char password[128];
	unsigned gid; //unique group id
	unsigned *group_list; //list of users which are in this group
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

//given a group, it checks if the user is in that group
int user_in_group(int gid, int uid, FileHandle *groups);