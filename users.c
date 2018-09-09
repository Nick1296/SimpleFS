#include "users.h"
#include "simplefs_shell_apis.h"
#include "disk_driver.h"

//all the operations must be executed as root user

//opens or create the file which stores the last uid and gid created by the fs
//return NULL on error
FileHandle *load_ids(DirectoryHandle *dh, Wallet *wallet) {
	int res;
	//we move into the root directory
	while (dh->directory != NULL) {
		res = shell_changeDir(dh, "..", wallet);
		if (res == FAILED) {
			return NULL;
		}
	}
	//we move into etc, if this directory does not exist we create it
	res = shell_changeDir(dh, "etc", wallet);
	if (res == FAILED) {
		res = shell_mkDir(dh, "etc", wallet);
		if (res != FAILED) {
			res = shell_changeDir(dh, "etc", wallet);
		}
	}
	if (res == FAILED) {
		return NULL;
	}
	//now we load the uid file or create it if it does not exists
	FileHandle *ids = shell_openFile(dh, "id", wallet);
	if (ids == NULL) {
		ids = shell_createFile(dh, "id", wallet);
	}
	return ids;
}

//given the handle to the id file we load its content on memory
//returns NULL on error
Ids *read_ids(FileHandle *id, Wallet *wallet) {
	int res;
	//we seek at the beginning of the file to be sure to override everything
	res = shell_seekFile(id, 0, wallet);
	if (res == FAILED || res == PERM_ERR) {
		return NULL;
	}
	//now we allocate the Ids struct
	Ids *ids = (Ids *) malloc(sizeof(Ids));
	//we read the uid from file
	res = shell_readFile(id, &(ids->last_uid), sizeof(int), wallet);
	if (res == FAILED || res == PERM_ERR || res != sizeof(int)) {
		free(ids);
		return NULL;
	}
	
	//we read the gid from file
	res = shell_readFile(id, &(ids->last_gid), sizeof(int), wallet);
	if (res == FAILED || res == PERM_ERR || res != sizeof(int)) {
		free(ids);
		return NULL;
	}
	return ids;
}

//given the handle to the id file we save on it the ids in the wallet
int save_ids(FileHandle *id, Wallet *wallet) {
	int res;
	//we seek at the beginning of the file to be sure to override everything
	res = shell_seekFile(id, 0, wallet);
	if (res == FAILED || res == PERM_ERR) {
		return res;
	}
	//we write the uid to file
	res = shell_writeFile(id, &(wallet->ids->last_uid), sizeof(int), wallet);
	if (res == FAILED || res == PERM_ERR) {
		return res;
	}
	
	//we write the gid to file
	res = shell_writeFile(id, &(wallet->ids->last_gid), sizeof(int), wallet);
	if (res == FAILED || res == PERM_ERR) {
		return res;
	}
	return res;
}

//given the fs root it opens the file containing users
//returns NULL on error
FileHandle *load_users(DirectoryHandle *dh, Wallet *wallet) {
	int res;
	//we check that the given directory handle is the one of the root directory
	while (dh->directory != NULL) {
		res = shell_changeDir(dh, "..", wallet);
		if (res == FAILED) {
			return NULL;
		}
	}
	//we move into etc, if this directory does not exist we create it
	res = shell_changeDir(dh, "etc", wallet);
	if (res == FAILED) {
		res = shell_mkDir(dh, "etc", wallet);
		if (res != FAILED) {
			res = shell_changeDir(dh, "etc", wallet);
		}
	}
	if (res == FAILED) {
		return NULL;
	}
	//now we load the passwd file or create it if it does not exists
	FileHandle *usr = shell_openFile(dh, "passwd", wallet);
	if (usr == NULL) {
		usr = shell_createFile(dh, "passwd", wallet);
	}
	return usr;
}


//given a file handle creates a User list
ListHead *read_users(FileHandle *users, Wallet *wallet) {
	int res;
	//we seek at the beginning of the file to be sure to read everything
	res = shell_seekFile(users, 0, wallet);
	if (res == FAILED || res == PERM_ERR) {
		return NULL;
	}
	//we create the ListHead
	ListHead *usrlst = (ListHead *) malloc(sizeof(ListHead));
	usrlst->first = NULL;
	usrlst->last = NULL;
	
	//we create the first list element
	ListElement *lst = (ListElement *) malloc(sizeof(ListElement));
	//we initialize the list element
	lst->prev = NULL;
	lst->next = NULL;
	lst->item = NULL;
	//we add it in the list head
	usrlst->first = lst;
	//we allocate the first user element
	User *tmp = (User *) malloc(sizeof(User));
	//this variable indicates if the last read operation was successful
	int usr_read = SUCCESS;
	//we read all the user in the file
	while (usr_read == SUCCESS) {
		//we read a user from the file and fill it into a User element
		res = shell_readFile(users, tmp, sizeof(User), wallet);
		if (res == sizeof(User)) {
			//we fit the user into the list
			lst->item = tmp;
			//we create and initialize the new list element
			lst->next = (ListElement *) malloc(sizeof(ListElement));
			lst->next->prev = lst;
			lst = lst->next;
			lst->item = NULL;
			tmp = (User *) malloc(sizeof(User));
			lst->next = NULL;
			usr_read = SUCCESS;
		} else {
			usr_read = FAILED;
		}
	}
	//we deallocate the last list element because it's empty when we have finished reading from file
	free(tmp);
	if (lst->prev != NULL) {
		lst = lst->prev;
		free(lst->next);
		lst->next = NULL;
		//we save the last element in the ListHead
		usrlst->last = lst;
	} else {
		//our list is empty, so we deallocate the list head
		free(lst);
		free(usrlst);
		usrlst = NULL;
	}
	if (res != FAILED) {
		return usrlst;
	} else {
		delete_list(usrlst);
	}
	return NULL;
}

//it saves the users list into the file
int save_users(FileHandle *users, ListHead *data, Wallet *wallet) {
	if (data == NULL) {
		return FAILED;
	}
	ListElement *lst = data->first;
	int res;
	//we seek at the beginning of the file to be sure to override everything
	res = shell_seekFile(users, 0, wallet);
	if (res == FAILED || res == PERM_ERR) {
		return res;
	}
	User *usr;
	while (lst != NULL && res != FAILED) {
		usr = lst->item;
		if (usr != NULL) {
			res = shell_writeFile(users, usr, sizeof(User), wallet);
			lst = lst->next;
		}
	}
	if (res == FAILED || res == PERM_ERR) {
		return res;
	} else {
		return SUCCESS;
	}
}

// adds a new user
//new users are always added in the tail of the list
int useradd(char *username, DirectoryHandle *dh, Wallet *wallet) {
	//check if the given string respects the memory limit
	if (strlen(username) > NAME_LENGTH || username == NULL) {
		return FAILED;
	}
	// we check if the user list is invalid
	if (wallet->user_list == NULL) {
		return FAILED;
	}
	int current_user = wallet->current_user->uid;
	//we check if we have enough permissions
	if (current_user != ROOT) {
		return PERM_ERR;
	}
	//we check if the username it's already in use
	ListElement *usr_lst = usrsrc(wallet, username, 0);
	if (usr_lst != NULL) {
		return FAILED;
	}
	//we get the last item in the user list
	ListElement *lst = wallet->user_list->last;
	//if the username it's not used we calculate a new uid and (the last uid+1)
	
	//we create the user group
	int res = groupadd(username, wallet);
	if (res == FAILED || res == PERM_ERR) {
		return res;
	}
	//we now create the new user
	User *new_usr = (User *) malloc(sizeof(User));
	memset(new_usr->account, '\0', NAME_LENGTH * sizeof(char));
	memcpy(new_usr->account, username, strlen(username));
	new_usr->uid = wallet->ids->last_uid + 1;
	new_usr->gid = res;
	
	//we create the new list Element
	ListElement *new_lst = (ListElement *) malloc(sizeof(ListElement));
	new_lst->prev = lst;
	new_lst->next = NULL;
	new_lst->item = new_usr;
	lst->next = new_lst;
	//we update the ListHead
	wallet->user_list->last = new_lst;
	
	//we save the user list into its file
	res = save_users(wallet->user_file, wallet->user_list, wallet);
	if (res == FAILED || res == PERM_ERR) {
		lst->next = NULL;
		free(new_usr);
		return res;
	}
	//now we add the new user in its group
	res = gpasswd(username, username, wallet, ADD);
	if (res == FAILED || res == PERM_ERR) {
		//we delete the created user and return  the error
		userdel(username, dh, wallet);
		return res;
	}
	//now we update the Ids structure
	wallet->ids->last_uid++;
	res = save_ids(wallet->ids_file, wallet);
	if (res == FAILED || res == PERM_ERR) {
		userdel(username, dh, wallet);
		return res;
	}
	//now we need to create the user folder
	//we get a new directory handle to /
	DirectoryHandle *dh2 = NULL;
	//this variabile controla if we must deallocate the handle at the end of the function
	int to_be_freed = 0;
	if (dh->directory == NULL) {
		//if we are in the root directory we use this handle
		dh2 = dh;
	} else {
		//or we get tha handle to the root directory
		dh2 = fs_init(dh->sfs, dh->sfs->disk, wallet->current_user->uid, wallet->current_user->gid);
		to_be_freed = 1;
	}
	//we try to move into the directory "home"
	res = shell_changeDir(dh2, "home", wallet);
	if (res == FAILED || res == PERM_ERR) {
		userdel(username, dh, wallet);
		return res;
	}
	//we create the user directory
	res = shell_mkDir(dh2, username, wallet);
	if (res == FAILED || res == PERM_ERR) {
		userdel(username, dh, wallet);
		return res;
	}
	//we make it owned by the created user
	res = shell_chown(dh2, username, username, wallet);
	if (res == FAILED || res == PERM_ERR) {
		userdel(username, dh, wallet);
		shell_removeFile(dh2, username, wallet);
	}
	//we deallocate the obtained handle
	//TODO: check these free
	if (to_be_freed) {
		free(dh2->current_block);
		free(dh2->dcb);
		free(dh2->directory);
		free(dh2);
	}
	return res;
}

// deletes a user
int userdel(char *username, DirectoryHandle *dh, Wallet *wallet) {
	//check if the given string respects the memory limit
	if (strlen(username) > NAME_LENGTH || username == NULL) {
		return FAILED;
	}
	//we check if we have enough permissions
	int res;
	int current_user = wallet->current_user->uid;
	if (current_user != ROOT) {
		return PERM_ERR;
	}
	//we check if the user list is valid
	if (wallet->user_list == NULL) {
		return FAILED;
	}
	//we now try to find the user to delete (if exists)
	ListElement *usr_lst = usrsrc(wallet, username, 0);
	User *usr = NULL;
	if (usr_lst != NULL) {
		usr = (User *) usr_lst->item;
	}
	//if we found the user we delete it and update the file
	if (usr != NULL) {
		//before deleting the user we check if the user primary group contains other users
		ListElement *grp_lst = grpsrc(wallet, username, usr->gid);
		Group *grp = (Group *) grp_lst->item;
		//if the user primary group contains other users we can't delete it
		if (grp->group_members[0] == usr->uid && grp->group_members[1] == MISSING) {
			res = groupdel(username, wallet);
			if (res == FAILED || res == PERM_ERR) {
				return res;
			}
		}
		//now we delete the user
		ListElement *prev = NULL, *next = NULL;
		if (usr_lst->prev == NULL) {
			//we delete the first entry in the list
			next = usr_lst->next;
			if (next != NULL) {
				next->prev = NULL;
			}
			wallet->user_list->first = next;
		} else {
			if (usr_lst->next == NULL) {
				//we delete the last entry in the list
				prev = usr_lst->prev;
				if (prev != NULL) {
					prev->next = NULL;
				}
				wallet->user_list->last = prev;
			} else {
				//we delete an entry in between the list
				prev = usr_lst->prev;
				next = usr_lst->next;
				prev->next = next;
				next->prev = prev;
			}
		}
		//we deallocate the memory block
		free(usr_lst->item);
		free(usr_lst);
		//we save the changes in the file
		res = save_users(wallet->user_file, wallet->user_list, wallet);
		if (res == FAILED || res == PERM_ERR) {
			//we rollback the changes by reloading the list
			delete_list(wallet->user_list);
			wallet->user_list = read_users(wallet->user_file, wallet);
			CHECK_ERR(wallet->user_list == NULL, "users subsystem compromised!")
		}
		//now we need to remove the user folder
		//we get a new directory handle to /
		DirectoryHandle *dh2 = NULL;
		//this variabile controla if we must deallocate the handle at the end of the function
		int to_be_freed = 0;
		if (dh->directory == NULL) {
			//if we are in the root directory we use this handle
			dh2 = dh;
		} else {
			//or we get the handle to the root directory
			dh2 = fs_init(dh->sfs, dh->sfs->disk, wallet->current_user->uid, wallet->current_user->gid);
			to_be_freed = 1;
		}
		//we remove the user home directory
		res = shell_removeFile(dh2, username, wallet);
		if (res == PERM_ERR) {
			return res;
		}
		//we deallocate the obtained handle
		//TODO: check these free
		if (to_be_freed) {
			free(dh2->current_block);
			free(dh2->dcb);
			free(dh2->directory);
			free(dh2);
		}
		return res;
	} else {
		//otherwise we return failed
		return FAILED;
	}
}

//given a username or uid it searches the corresponding User
ListElement *usrsrc(Wallet *wallet, char *name, int uid) {
	//we check if we have valid data
	if ((uid == MISSING && name == NULL) || wallet == NULL || wallet->user_list == NULL) {
		return NULL;
	}
	if (name != NULL) {
		if (strlen(name) > NAME_LENGTH) {
			return NULL;
		}
	}
	ListElement *lst = wallet->user_list->first;
	int found = 0;
	//we search the list by username
	if (name != NULL) {
		while (lst != NULL && !found) {
			if (memcmp(name, ((User *) lst->item)->account, strlen(name)) == 0) {
				found = 1;
			} else {
				lst = lst->next;
			}
		}
	} else {
		//we search ny uid
		while (lst != NULL && !found) {
			if (((User *) lst->item)->uid == uid) {
				found = 1;
			} else {
				lst = lst->next;
			}
		}
	}
	//we return the result
	if (found) {
		return lst;
	} else {
		return NULL;
	}
}

//given a username or gid it searches the corresponding Group
ListElement *grpsrc(Wallet *wallet, char *name, int gid) {
	//we check if we have valid data
	if ((gid == MISSING && name == NULL) || wallet == NULL || wallet->group_list == NULL) {
		return NULL;
	}
	if (name != NULL) {
		if (strlen(name) > NAME_LENGTH) {
			return NULL;
		}
	}
	ListElement *lst = wallet->group_list->first;
	int found = 0;
	Group *grp;
	//we search the list by name of the group
	if (name != NULL) {
		while (lst != NULL && lst->item != NULL && !found) {
			grp = lst->item;
			if (memcmp(name, grp->group_name, strlen(name)) == 0) {
				found = 1;
			} else {
				lst = lst->next;
			}
		}
	} else {
		//we search by gid
		while (lst != NULL && lst->item != NULL && !found) {
			grp = lst->item;
			if (grp->gid == gid) {
				found = 1;
			} else {
				lst = lst->next;
			}
		}
	}
	
	//we return the result
	if (found) {
		return lst;
	} else {
		return NULL;
	}
}

//given the fs root it opens the file containing groups
// returns NULL on error
FileHandle *load_groups(DirectoryHandle *dh, Wallet *wallet) {
	int res;
	//we check that the given directory handle is the one of the root directory
	while (dh->directory != NULL) {
		res = shell_changeDir(dh, "..", wallet);
		if (res == FAILED) {
			return NULL;
		}
	}
	//we move into etc, if this directory does not exist we create it
	res = shell_changeDir(dh, "etc", wallet);
	if (res == FAILED) {
		res = shell_mkDir(dh, "etc", wallet);
		if (res != FAILED) {
			res = shell_changeDir(dh, "etc", wallet);
		}
	}
	if (res == FAILED) {
		return NULL;
	}
	//now we load the group file or create it if it does not exists
	FileHandle *usr = shell_openFile(dh, "group", wallet);
	if (usr == NULL) {
		usr = shell_createFile(dh, "group", wallet);
	}
	return usr;
}

//given a file handle creates a group list
ListHead *read_groups(FileHandle *groups, Wallet *wallet) {
	int res;
	//we seek at the beginning of the file to be sure to override everything
	res = shell_seekFile(groups, 0, wallet);
	if (res == FAILED || res == PERM_ERR) {
		return NULL;
	}
	//we create our list head
	ListHead *grp_lst = (ListHead *) malloc(sizeof(ListHead));
	//we allocate memory for the first list element containing the group
	ListElement *lst = (ListElement *) malloc(sizeof(ListElement));
	lst->item = (Group *) malloc(sizeof(Group));
	lst->next = NULL;
	lst->prev = NULL;
	grp_lst->first = lst;
	int grp_read = SUCCESS;
	//we read all the user in the file
	while (grp_read == SUCCESS) {
		
		//we read a user from the file and fill it into a User element
		res = shell_readFile(groups, lst->item, sizeof(Group), wallet);
		if (res == sizeof(Group)) {
			//we create a new list element to store the next group
			lst->next = malloc(sizeof(ListElement));
			lst->next->prev = lst;
			lst = lst->next;
			lst->item = malloc(sizeof(Group));
			lst->next = NULL;
			grp_read = SUCCESS;
		} else {
			grp_read = FAILED;
		}
	}
	//we clear the unused list item
	free(lst->item);
	if (lst->prev != NULL) {
		lst = lst->prev;
		free(lst->next);
		lst->next = NULL;
		grp_lst->last = lst;
	} else {
		//we don't have items in the file, so we deallocate our ListHead
		free(lst);
		free(grp_lst);
		grp_lst = NULL;
	}
	
	if (res != FAILED) {
		return grp_lst;
	} else {
		//cleaning up in case of error
		delete_list(grp_lst);
	}
	return NULL;
}

//it saves the users list into the file
int save_groups(FileHandle *groups, ListHead *data, Wallet *wallet) {
	if (data == NULL) {
		return FAILED;
	}
	ListElement *lst = data->first;
	int res;
	//we seek at the beginning of the file to be sure to override everything
	res = shell_seekFile(groups, 0, wallet);
	if (res == FAILED || res == PERM_ERR) {
		return res;
	}
	while (lst != NULL && res != FAILED) {
		res = shell_writeFile(groups, lst->item, sizeof(Group), wallet);
		if (res == sizeof(Group)) {
			lst = lst->next;
		} else {
			res = FAILED;
		}
	}
	if (res == FAILED || res == PERM_ERR) {
		return res;
	} else {
		return SUCCESS;
	}
}

//check if a user is in a group
int usringrp(User *usr, char *grp_name, int gid, Wallet *wallet) {
	
	//we check if we have valid parameters
	if (((grp_name == NULL || strlen(grp_name) > NAME_LENGTH) && gid == MISSING) || wallet == NULL || usr == NULL) {
		return FAILED;
	}
	//we find the group
	ListElement *lst = grpsrc(wallet, grp_name, gid);
	Group *grp = NULL;
	if (lst != NULL) {
		grp = lst->item;
	}
	if (grp == NULL) {
		return FAILED;
	}
	int i = 0;
	while (i < GROUP_SIZE) {
		if (usr->uid == grp->group_members[i]) {
			return SUCCESS;
		} else {
			if (grp->group_members[i] == MISSING) {
				return FAILED;
			} else {
				i++;
			}
		}
	}
	return FAILED;
}

// creates a group, returns the group gid on success
int groupadd(char *name, Wallet *wallet) {
	//check if we have valid parameters
	if (name == NULL || strlen(name) > NAME_LENGTH || wallet == NULL || wallet->group_list == NULL ||
	    wallet->group_file == NULL) {
		return FAILED;
	}
	//we search for a group with the same name
	if (grpsrc(wallet, name, MISSING) != NULL) {
		return FAILED;
	}
	//we check if we have enough permissions
	if (wallet->current_user->uid != ROOT) {
		return PERM_ERR;
	}
	//we need to generate a new gid, which must be unique, to do so we get the last generated gid+1
	//we get the last created group
	ListElement *lst = wallet->group_list->last;
	//now we create a new group
	Group *new_grp = (Group *) malloc(sizeof(Group));
	new_grp->gid = wallet->ids->last_gid + 1;
	//we set the new group as empty
	memset(new_grp->group_members, MISSING, GROUP_SIZE * sizeof(int));
	memset(new_grp->group_name, '\0', NAME_LENGTH * sizeof(char));
	memcpy(new_grp->group_name, name, strlen(name));
	
	//we create the group list element
	ListElement *new_lst = (ListElement *) malloc(sizeof(ListElement));
	new_lst->item = new_grp;
	new_lst->next = NULL;
	new_lst->prev = lst;
	lst->next = new_lst;
	
	//we update the list head
	wallet->group_list->last = new_lst;
	
	//now we save the group list
	int res = save_groups(wallet->group_file, wallet->group_list, wallet);
	if (res == FAILED || res == PERM_ERR) {
		//we revert the changes
		lst->next = NULL;
		free(new_grp);
		free(new_lst);
		wallet->user_list->last = lst;
		return res;
	}
	
	//we save our last used gid
	wallet->ids->last_gid++;
	res = save_ids(wallet->ids_file, wallet);
	CHECK_ERR(res == FAILED || res == PERM_ERR, "can't update the ids file");
	return new_grp->gid;
}

//deletes a group
int groupdel(char *name, Wallet *wallet) {
	// check if we have the right parameters
	if (name == NULL || strlen(name) > NAME_LENGTH || wallet == NULL || wallet->group_list == NULL ||
	    wallet->group_file == NULL) {
		return FAILED;
	}
	//we check if we have the right permission
	if (wallet->current_user->uid != ROOT) {
		return PERM_ERR;
	}
	//we now search for the group to delete
	ListElement *grp_lst = grpsrc(wallet, name, MISSING);
	if (grp_lst == NULL) {
		return FAILED;
	}
	//now we delete the user
	ListElement *prev = NULL, *next = NULL;
	if (grp_lst->prev == NULL) {
		//we delete the first entry in the list
		next = grp_lst->next;
		if (next != NULL) {
			next->prev = NULL;
		}
		//we update our list head
		wallet->group_list->first = next;
	} else {
		if (grp_lst->next == NULL) {
			//we delete the last entry in the list
			prev = grp_lst->prev;
			if (prev != NULL) {
				prev->next = NULL;
			}
			//we update the list head
			wallet->group_list->last = prev;
		} else {
			//we delete an entry in between the list
			prev = grp_lst->prev;
			next = grp_lst->next;
			prev->next = next;
			next->prev = prev;
		}
	}
	//we deallocate the memory blocks
	free(grp_lst->item);
	free(grp_lst);
	//we save the changes in the file
	int res = save_groups(wallet->group_file, wallet->group_list, wallet);
	if (res == FAILED || res == PERM_ERR) {
		//we rollback the changes by reloading the list
		delete_list(wallet->group_list);
		wallet->group_list = read_groups(wallet->group_file, wallet);
		CHECK_ERR(wallet->group_list == NULL, "users subsystem compromised!")
		return res;
	}
	return res;
}

//adds or deletes a user from a group
int gpasswd(char *group, char *user, Wallet *wallet, int type) {
	// we check if we are given correct parameters
	if (group == NULL || strlen(group) > NAME_LENGTH || user == NULL || strlen(user) > NAME_LENGTH || wallet == NULL ||
	    (type != ADD && type != REMOVE)) {
		return FAILED;
	}
	//check if we have the required permissions
	if (wallet->current_user->uid != ROOT) {
		return PERM_ERR;
	}
	//we search for the group in which perform the operation
	ListElement *grp_lst = grpsrc(wallet, group, MISSING);
	if (grp_lst == NULL) {
		return FAILED;
	}
	Group *grp = grp_lst->item;
	if (grp == NULL) {
		return FAILED;
	}
	//we search for the user in which perform the operation
	ListElement *usr_lst = usrsrc(wallet, user, MISSING);
	if (usr_lst == NULL) {
		return FAILED;
	}
	User *usr = usr_lst->item;
	if (usr == NULL) {
		return FAILED;
	}
	//we perform the operation
	if (type == ADD) {
		//we add a user into a given group
		
		//we search for a empty slot in the group while making sure the user isn't already in the group
		int i;
		for (i = 0; grp->group_members[i] != MISSING && i < GROUP_SIZE; i++) {
			//we check if the user is already in the group
			if (grp->group_members[i] == usr->uid) {
				return FAILED;
			}
		}
		//we check if we can fit another user in the group
		if (grp->group_members[i] != MISSING) {
			return FAILED;
		}
		//we insert the user in the group
		grp->group_members[i] = usr->uid;
	}
	if (type == REMOVE) {
		//we remove a user from the given group
		//we search fo the user in the group
		int i, hole = MISSING;
		for (i = 0; grp->group_members[i] != MISSING && i < GROUP_SIZE; i++) {
			if (grp->group_members[i] == usr->uid) {
				//if we have found the user we mark its location to be filled with the last user in the list
				hole = i;
			}
		}
		//we check if the user has been found
		if (hole == MISSING) {
			return FAILED;
		}
		//if the user to be removed isn't the last one in the list
		if (hole < i) {
			grp->group_members[hole] = grp->group_members[i];
			grp->group_members[i] = MISSING;
		} else {
			grp->group_members[hole] = MISSING;
		}
	}
	//now we save the changes into the file
	int res = save_groups(wallet->group_file, wallet->group_list, wallet);
	if (res == FAILED || res == PERM_ERR) {
		//we rollback the changes by reloading the list
		delete_list(wallet->group_list);
		wallet->group_list = read_groups(wallet->group_file, wallet);
		CHECK_ERR(wallet->group_list == NULL, "users subsystem compromised!")
	}
	return res;
}

//given a wallet deallocate it along with its content
void destroy_wallet(Wallet *wallet) {
	if (wallet != NULL) {
		//we close the handle to /etc/ids
		if (wallet->ids_file != NULL) {
			shell_closeFile(wallet->ids_file);
		}
		//we close the handle to /etc/group
		if (wallet->group_file != NULL) {
			shell_closeFile(wallet->group_file);
		}
		//we close the handle to /etc/passwd
		if (wallet->user_file != NULL) {
			shell_closeFile(wallet->user_file);
		}
		//we delete the group list
		if (wallet->group_list != NULL) {
			delete_list(wallet->group_list);
		}
		//we delete the file list
		if (wallet->user_list != NULL) {
			delete_list(wallet->user_list);
		}
		//we delete the last used ids
		if (wallet->ids != NULL) {
			free(wallet->ids);
		}
		//we delete the wallet
		free(wallet);
	}
}

//load the user subsytem and if there are no users in the system creates root user
//the directory handle given must be the root of the disk
Wallet *initialize_wallet(DirectoryHandle *dh) {
	int res;
	//create the root user
	User *root = (User *) malloc(sizeof(User));
	memset(root->account, '\0', NAME_LENGTH * sizeof(char));
	strcpy(root->account, "root\0");
	root->uid = ROOT;
	root->gid = ROOT;
	
	//initialize the wallet only with the temporary root user
	Wallet *wallet = (Wallet *) malloc(sizeof(Wallet));
	wallet->current_user = root;
	wallet->group_file = NULL;
	wallet->group_list = NULL;
	wallet->user_file = NULL;
	wallet->user_list = NULL;
	// try to load the users from the file
	wallet->user_file = load_users(dh, wallet);
	if (wallet->user_file == NULL) {
		destroy_wallet(wallet);
		return NULL;
	}
	wallet->user_list = read_users(wallet->user_file, wallet);
	//we check if the file was empty
	if (wallet->user_list == NULL) {
		//if the file was empty
		wallet->user_list = malloc(sizeof(ListHead));
		wallet->user_list->first = malloc(sizeof(User));
		wallet->user_list->first->item = root;
		wallet->user_list->first->next = NULL;
		wallet->user_list->first->prev = NULL;
		wallet->user_list->last = wallet->user_list->first;
		
		//we save our user list on file
		res = save_users(wallet->user_file, wallet->user_list, wallet);
		if (res == FAILED || res == PERM_ERR) {
			destroy_wallet(wallet);
			return NULL;
		}
	} else {
		//we load the root user on file
		free(wallet->current_user);
		ListElement *tmp_lst = usrsrc(wallet, NULL, ROOT);
		User *tmp;
		if (tmp_lst != NULL) {
			tmp = tmp_lst->item;
			wallet->current_user = tmp;
		}
	}
	//now we load the groups from the group file
	wallet->group_file = load_groups(dh, wallet);
	if (wallet->group_file == NULL) {
		destroy_wallet(wallet);
		return NULL;
	}
	//we load the group list
	wallet->group_list = read_groups(wallet->group_file, wallet);
	if (wallet->group_list == NULL) {
		//create the root group if the group file is empty
		Group *root_grp = (Group *) malloc(sizeof(Group));
		memset(root_grp->group_name, '\0', NAME_LENGTH * sizeof(char));
		strcpy(root_grp->group_name, "root");
		root_grp->gid = ROOT;
		memset(root_grp->group_members, MISSING, GROUP_SIZE * sizeof(int));
		root_grp->group_members[0] = ROOT;
		
		wallet->group_list = (ListHead *) malloc(sizeof(ListHead));
		wallet->group_list->first = (ListElement *) malloc(sizeof(ListElement));
		wallet->group_list->first->item = root_grp;
		wallet->group_list->first->next = NULL;
		wallet->group_list->first->prev = NULL;
		wallet->group_list->last = wallet->group_list->first;
		//we save the group list  on file
		res = save_groups(wallet->group_file, wallet->group_list, wallet);
		if (res == FAILED || res == PERM_ERR) {
			destroy_wallet(wallet);
			return NULL;
		}
	}
	
	//we load the last used ids from the ids file
	wallet->ids_file = load_ids(dh, wallet);
	if (wallet->ids_file == NULL) {
		destroy_wallet(wallet);
		return NULL;
	}
	//now we try to read from the ids file
	wallet->ids = read_ids(wallet->ids_file, wallet);
	if (wallet->ids == NULL) {
		//we initialize and save the last used uid and gid
		wallet->ids = malloc(sizeof(Ids));
		wallet->ids->last_uid = ROOT;
		wallet->ids->last_gid = ROOT;
		res = save_ids(wallet->ids_file, wallet);
		if (res == FAILED || res == PERM_ERR) {
			destroy_wallet(wallet);
			return NULL;
		}
	}
	res = SUCCESS;
	//we go back to the root directory
	while (dh->directory != NULL && res != FAILED && res != PERM_ERR) {
		res = shell_changeDir(dh, "..", wallet);
	}
	//we check if the root folder is present
	res = shell_changeDir(dh, "root", wallet);
	if (res == PERM_ERR) {
		destroy_wallet(wallet);
		return NULL;
	}
	if (res == FAILED) {
		//we create the root folder
		res = shell_mkDir(dh, "root", wallet);
		if (res == FAILED || res == PERM_ERR) {
			destroy_wallet(wallet);
			return NULL;
		}
	}
	//we go back to /
	res = shell_changeDir(dh, "..", wallet);
	if (res == PERM_ERR || res == FAILED) {
		destroy_wallet(wallet);
		return NULL;
	}
	//we repeat the operation for the home folder
	
	res = shell_changeDir(dh, "home", wallet);
	if (res == PERM_ERR) {
		destroy_wallet(wallet);
		return NULL;
	}
	if (res == FAILED) {
		res = shell_mkDir(dh, "home", wallet);
		if (res == PERM_ERR || res == FAILED) {
			destroy_wallet(wallet);
			return NULL;
		}
	}
	//we go back to /
	res = shell_changeDir(dh, "..", wallet);
	if (res == PERM_ERR || res == FAILED) {
		destroy_wallet(wallet);
		return NULL;
	}
	return wallet;
}

// reads in the (preallocated) blocks array, the name of all files in a directory showing permissions
int shell_readDir_perms(char **names, DirectoryHandle *d, Wallet *wallet) {

	char newLine[] = "\n   ";
	char str[] = "directory empty\n";

	int FDB_max_elements = (BLOCK_SIZE - sizeof(BlockHeader) - sizeof(FileControlBlock) - sizeof(int)) / sizeof(int);
	int Dir_Block_max_elements = (BLOCK_SIZE - sizeof(BlockHeader)) / sizeof(int);
	int dir_entries = d->dcb->num_entries;
	int i, res, pos = 0;

	size_t dim_names = (dir_entries + 1) * (((NAME_LENGTH * 2) + 50 + FILENAME_MAX_LENGTH) * sizeof(char));
	*names = (char *) malloc(dim_names);
	memset(*names, 0, dim_names);

	strncpy(*names, d->dcb->fcb.name, strlen(d->dcb->fcb.name));
	strcat(*names, newLine);

	if (dim_names == 1 * ((FILENAME_MAX_LENGTH + 50) * sizeof(char))) {
		strcat(*names, str);
		return SUCCESS;
	}
	FirstDirectoryBlock *file = (FirstDirectoryBlock *) malloc(sizeof(FirstDirectoryBlock));
	DirectoryBlock *db = (DirectoryBlock *) malloc(sizeof(DirectoryBlock));

	// We search for all the files in the current directory block
	for (i = 0; i < dir_entries; i++) {
		// We get the file
		memset(file, 0, sizeof(FirstDirectoryBlock));
		if (i < FDB_max_elements) {
			res = DiskDriver_readBlock(d->sfs->disk, file, d->dcb->file_blocks[i]);
			CHECK_ERR(res == FAILED, "Error read dir block in simplefs_readDir");
		} else {
			// File contenuto nei DB della directory genitore
			int logblock = ((i - FDB_max_elements) / Dir_Block_max_elements) + 1;
			int j, block = d->dcb->header.next_block;
			if (pos != logblock) memset(db, 0, sizeof(DirectoryBlock));
			for (j = 0; j < logblock && pos != logblock; j++) {
				res = DiskDriver_readBlock(d->sfs->disk, db, block);
				CHECK_ERR(res == FAILED, "Error on read of DirectoryBlock in simpleFS_readDir");
				block = db->header.next_block;
				pos = j + 1;
			}
			res = DiskDriver_readBlock(d->sfs->disk, file, db->file_blocks[(i - FDB_max_elements) % Dir_Block_max_elements]);
			CHECK_ERR(res == FAILED, "Error read dir block in simplefs_readDir");
		}

		// Copy data of files of current directory
		// we print the - if it's a file or d if it's a directory
		if (file->fcb.is_dir == DIRECTORY) sprintf((*names) + strlen(*names), "d");
		else sprintf((*names) + strlen(*names), "-");
		//we print the permissions on the file
		int j, perms[3] = {file->fcb.permissions.user, file->fcb.permissions.group, file->fcb.permissions.others};
		for (j = 0; j < 3; j++) {
			if (perms[j] >= 4) {
				sprintf((*names) + strlen(*names), "r");
			} else {
				sprintf((*names) + strlen(*names), "-");
			}
			if (perms[j] >= 6 || perms[j] == 2) {
				sprintf((*names) + strlen(*names), "w");
			} else {
				sprintf((*names) + strlen(*names), "-");
			}
			if (perms[j] == 7 || perms[j] == 3 || perms[j] == 1) {
				sprintf((*names) + strlen(*names), "x");
			} else {
				sprintf((*names) + strlen(*names), "-");
			}
		}
		ListElement *lst = NULL;
		User *owner = NULL;
		Group *group = NULL;
		lst = usrsrc(wallet, NULL, file->fcb.permissions.user_uid);
		if (lst != NULL) {
			owner = lst->item;
		}
		lst = grpsrc(wallet, NULL, file->fcb.permissions.group_uid);
		if (lst != NULL) {
			group = lst->item;
		}
		if (owner == NULL || group == NULL) {
			return FAILED;
		}
		sprintf((*names) + strlen(*names), " %s %s ", owner->account, group->group_name);
		sprintf((*names) + strlen(*names), "%d", file->fcb.size_in_bytes);
		sprintf((*names) + strlen(*names), "%c", ' ');
		strncat((*names) + strlen(*names), file->fcb.name, strlen(file->fcb.name));
		strcat(*names, newLine);
	}

	memset(*names + strlen(*names), 0, 1);
	free(file);
	free(db);
	return SUCCESS;
}
