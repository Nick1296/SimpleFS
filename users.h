#pragma once

#include "simplefs.h"

typedef struct _User {
	char *account;
	char *password;
	unsigned uid;
	unsigned gid;
} User;

typedef struct _Group {
	char *group_name;
	char *password;
	unsigned gid;
	unsigned *group_list;
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