#include "users.h"
#include "simplefs_apis.h"

//all the operations must be executed as root user

//given the fs root it opens the file containing users
//returns NULL on error
FileHandle *load_users(DirectoryHandle *dh, Wallet *wallet) {
	int res;
	User* current_user=wallet->current_user;
	//we check that the given directory handle is the one of the root directory
	while (dh->directory != NULL) {
		res = changeDir(dh, "..", current_user->uid, usringrp(current_user,grpsrc(wallet,NULL,dh->dcb->fcb.permissions.group_uid)));
		if (res == FAILED) {
			return NULL;
		}
	}
	//we move into etc, if this directory does not exist we create it
	res = changeDir(dh, "etc", current_user->uid,
	                usringrp(current_user, grpsrc(wallet, NULL, dh->dcb->fcb.permissions.group_uid)));
	if (res == FAILED) {
		res = mkDir(dh, "etc", current_user->uid,
		            usringrp(current_user, grpsrc(wallet, NULL, dh->dcb->fcb.permissions.group_uid)));
		if (res != FAILED) {
			res = changeDir(dh, "etc", current_user->uid,
			                usringrp(current_user, grpsrc(wallet, NULL, dh->dcb->fcb.permissions.group_uid)));
		}
	}
	if (res == FAILED) {
		return NULL;
	}
	//now we load the passwd file or create it if it does not exists
	FileHandle *usr = openFile(dh, "passwd", current_user->uid,
	                           usringrp(current_user, grpsrc(wallet, NULL, dh->dcb->fcb.permissions.group_uid)));
	if(usr == NULL){
		usr=createFile(dh,"passwd", current_user->uid,
		               usringrp(current_user, grpsrc(wallet, NULL, dh->dcb->fcb.permissions.group_uid)));
	}
	return usr;
}

//given a file handle creates a User list
User *read_users(FileHandle *users, Wallet *wallet){
	int res;
	int usr_in_grp= usringrp(wallet->current_user, grpsrc(wallet, NULL, users->fcb->fcb.permissions.group_uid));
	unsigned current_user=wallet->current_user->uid;
	//we seek at the beginning of the file to be sure to override everything
	res = seekFile(users, 0, current_user,usr_in_grp);
	if (res == FAILED || res==PERM_ERR) {
		return NULL;
	}
	// we set res to enter the while
	res= sizeof(User)- (2*sizeof(User*));
	User *usr=NULL,*lst;
	usr = malloc(sizeof(User));
	lst=usr;
	lst->next = NULL;
	int usr_num=0;
	//we read all the user in the file
	while(res== sizeof(User) - (2*sizeof(User*))){
		
		//we read a user from the file and fill it into a User element
		res=readFile(users,lst, sizeof(User)-sizeof(User*),current_user,usr_in_grp);
		if(res== sizeof(User) - sizeof(User *)){
			lst->next = malloc(sizeof(User));
			lst = lst->next;
			lst->next=NULL;
			usr_num++;
		}
	}
	if(usr_num>0 && res!=FAILED){
		return usr;
	} else{
		//cleaning up in case of error
		User* tmp;
		while(usr!=NULL){
			tmp=usr;
			usr=usr->next;
			free(tmp);
		}
	}
	return NULL;
}

//it saves the users list into the file
int save_users(FileHandle *users, User *data, Wallet *wallet){
	User* lst=data;
	int res;
	int usr_in_grp = usringrp(wallet->current_user, grpsrc(wallet, NULL, users->fcb->fcb.permissions.group_uid));
	unsigned current_user=wallet->current_user->uid;
	//we seek at the beginning of the file to be sure to override everything
	res=seekFile(users,0,current_user,usr_in_grp);
	if(res==FAILED || res==PERM_ERR){
		return res;
	}
	while (lst!=NULL && res!=FAILED){
		res=writeFile(users,lst, sizeof(User)- sizeof(User*),current_user,usr_in_grp);
		if(res!=FAILED){
			lst=lst->next;
		}
	}
	if(res==FAILED || res==PERM_ERR){
		return res;
	}else{
		return SUCCESS;
	}
}

// adds a new user
//new users are always added in the tail of the list
int useradd(char *username,Wallet* wallet) {
	// we check if the user list is invalid
	if(wallet->user_list==NULL){
		return FAILED;
	}
	unsigned current_user=wallet->current_user->uid;
	//we check if we have enough permissions
	if(current_user!=ROOT){
		return PERM_ERR;
	}
	//we check if the username it's already in use
	User *usr=usrsrc(wallet,username,0);
	if(usr!=NULL){
		return FAILED;
	}
	//we get the user list
	User *lst=wallet->user_list;
	//if the username it's not used we calculate a new uid and (the last uid+1)
	//to get the last gid created we got to the last position in the list
	while(lst->next!=NULL){
		lst=lst->next;
	}
	//we create the user group
	int res= groupadd(username, wallet);
	if(res==FAILED || res==PERM_ERR){
		return res;
	}
	//we now create the new user
	User *new_usr=malloc(sizeof(User));
	strcpy(new_usr->account,username);
	new_usr->uid=lst->uid+1;
	new_usr->next=NULL;
	new_usr->password[0]='\0';
	new_usr->gid=(unsigned) res;
	new_usr->prev=lst;
	//we add the new usr in the list
	lst->next=new_usr;
	//we save the user list into its file
	res=save_users(wallet->user_file,wallet->user_list,wallet);
	if(res==FAILED || res==PERM_ERR){
		lst->next=NULL;
		free(new_usr);
		return res;
	}
	//now we add the new user in its group
	res=gpasswd(username,username,wallet,ADD);
	if(res==FAILED || res==PERM_ERR){
		//we delete the created user and return  the error
		userdel(username,wallet);
	}
	return res;
}

// deletes a user
int userdel(char *username,Wallet* wallet){
	//we check if we have enough permissions
	int res;
	unsigned current_user=wallet->current_user->uid;
	if(current_user!=ROOT){
		return PERM_ERR;
	}
	//we check if the user list is valid
	if(wallet->user_list==NULL){
		return FAILED;
	}
	//we now try to find the user to delete (if exists)
	User *usr=usrsrc(wallet,username,0);
	//if we found the user we delete it and update the file
	if(usr!=NULL){
		//before deleting the user we check if the user primary group contains other users
		Group *grp=grpsrc(wallet,username,usr->gid);
		//if the user primary group contains other users we can't delete it
		if(grp->group_members[0]==usr->uid && grp->group_members[1]==EMPTY){
			res=groupdel(username,wallet);
			if(res==FAILED || res==PERM_ERR){
				return res;
			}
		}
		//now we delete the user
		User *prev=NULL, *next=NULL;
		if(usr->prev==NULL){
			//we delete the first entry in the list
			next=usr->next;
			if(next!=NULL) {
				next->prev = NULL;
			}
		}else{
			if(usr->next==NULL){
				//we delete the last entry in the list
				prev=usr->prev;
				if(prev!=NULL) {
					prev->next = NULL;
				}
			}else{
				//we delete an entry in between the list
				prev = usr->prev;
				next = usr->next;
				prev->next = next;
				next->prev = prev;
			}
		}
		//we deallocate the memory block
		free(usr);
		//we save the changes in the file
		res=save_users(wallet->user_file,wallet->user_list,wallet);
		if (res == FAILED || res == PERM_ERR) {
			//we rollback the changes by reloading the list
			delete_user_list(wallet->user_list);
			wallet->user_list = read_users(wallet->user_file, wallet);
			CHECK_ERR(wallet->user_list == NULL, "users subsystem compromised!")
		}
		return res;
	}else{
		//otherwise we return failed
		return FAILED;
	}
}

//given a username or uid it searches the corresponding User
User *usrsrc(Wallet *wallet, char *name, unsigned uid){
	//we check if we have valid data
	if(wallet==NULL || (uid==EMPTY && name==NULL)){
		return NULL;
	}
	User *lst = wallet->user_list;
	int found = 0;
	//we search the list
	while (lst != NULL && !found) {
		if ((uid!=EMPTY || lst->uid == uid) || (name!=NULL && strcmp(name, lst->account) == 0)) {
			found = 1;
		} else {
			lst = lst->next;
		}
	}
	//we return the result
	if(found){
		return lst;
	}else{
		return NULL;
	}
}

//given a username or gid it searches the corresponding Group
Group *grpsrc(Wallet *wallet, char *name, unsigned gid) {
	//we check if we have valid data
	if ((gid==EMPTY && name == NULL) || wallet == NULL) {
		return NULL;
	}
	Group *lst = wallet->group_list;
	int found = 0;
	//we search the list
	while (lst != NULL && !found) {
		if ((gid!=EMPTY && lst->gid == gid) ||(name!=NULL && strcmp(name, lst->group_name) == 0)) {
			found = 1;
		} else {
			lst = lst->next;
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
	unsigned current_user=wallet->current_user->uid;
	//we check that the given directory handle is the one of the root directory
	while (dh->directory != NULL) {
		res = changeDir(dh, "..", current_user, usringrp(wallet->current_user,grpsrc(wallet,NULL,dh->dcb->fcb.permissions.group_uid)));
		if (res == FAILED) {
			return NULL;
		}
	}
	//we move into etc, if this directory does not exist we create it
	res = changeDir(dh, "etc", current_user,
	                usringrp(wallet->current_user, grpsrc(wallet, NULL, dh->dcb->fcb.permissions.group_uid)));
	if (res == FAILED) {
		res = mkDir(dh, "etc", current_user,
		            usringrp(wallet->current_user, grpsrc(wallet, NULL, dh->dcb->fcb.permissions.group_uid)));
		if (res != FAILED) {
			res = changeDir(dh, "etc", current_user,
			                usringrp(wallet->current_user, grpsrc(wallet, NULL, dh->dcb->fcb.permissions.group_uid)));
		}
	}
	if (res == FAILED) {
		return NULL;
	}
	//now we load the passwd file or create it if it does not exists
	FileHandle *usr = openFile(dh, "group", current_user,
	                           usringrp(wallet->current_user, grpsrc(wallet, NULL, dh->dcb->fcb.permissions.group_uid)));
	if (usr == NULL) {
		usr = createFile(dh, "group", current_user,
		                 usringrp(wallet->current_user, grpsrc(wallet, NULL, dh->dcb->fcb.permissions.group_uid)));
	}
	return usr;
}

//given a file handle creates a group list
Group *read_groups(FileHandle *groups, Wallet *wallet){
	int res;
	int usr_in_grp = usringrp(wallet->current_user, grpsrc(wallet, NULL, groups->fcb->fcb.permissions.group_uid));
	unsigned current_user=wallet->current_user->uid;
	//we seek at the beginning of the file to be sure to override everything
	res = seekFile(groups, 0, current_user, usr_in_grp);
	if (res == FAILED || res==PERM_ERR) {
		return NULL;
	}
	// we set res to enter the while
	res = sizeof(Group) - (2*sizeof(Group*));
	Group *grp = NULL, *lst;
	grp = malloc(sizeof(Group));
	lst = grp;
	lst->next = NULL;
	int usr_num = 0;
	//we read all the user in the file
	while (res == sizeof(Group) - (2*sizeof(Group*))) {
		
		//we read a user from the file and fill it into a User element
		res = readFile(groups, lst, sizeof(Group) - (2 * sizeof(Group *)), current_user, usr_in_grp);
		if (res == sizeof(Group) - (2 * sizeof(Group *))) {
			lst->next = malloc(sizeof(Group));
			lst = lst->next;
			lst->next = NULL;
			usr_num++;
		}
	}
	if (usr_num > 0 && res != FAILED) {
		return grp;
	} else {
		//cleaning up in case of error
		Group *tmp;
		while (grp != NULL) {
			tmp = grp;
			grp = grp->next;
			free(tmp);
		}
	}
	return NULL;
}

//it saves the users list into the file
int save_groups(FileHandle *groups, Group *data,Wallet *wallet) {
	Group *lst = data;
	int res;
	int usr_in_grp = usringrp(wallet->current_user, grpsrc(wallet, NULL, groups->fcb->fcb.permissions.group_uid));
	unsigned current_user=wallet->current_user->uid;
	//we seek at the beginning of the file to be sure to override everything
	res = seekFile(groups, 0, current_user, usr_in_grp);
	if (res == FAILED || res == PERM_ERR) {
		return res;
	}
	while (lst != NULL && res != FAILED) {
		res = writeFile(groups, lst, sizeof(Group) - (2*sizeof(Group *)), current_user, usr_in_grp);
		if (res != FAILED) {
			lst = lst->next;
		}
	}
	if (res == FAILED || res == PERM_ERR) {
		return res;
	} else {
		return SUCCESS;
	}
}

//check if a user is in a group
int usringrp(User *usr, Group *grp){
	//we check if we have valid parameters
	if(usr==NULL || grp==NULL){
		return FAILED;
	}
	int i=0;
	while(i<GROUP_SIZE){
		if(usr->uid==grp->group_members[i]){
			return SUCCESS;
		}else{
			if(grp->group_members[i]==EMPTY){
				return FAILED;
			}else{
				i++;
			}
		}
	}
	return FAILED;
}

// creates a group
int groupadd(char *name, Wallet *wallet){
	//check if we have valid parameters
	if(name==NULL || wallet==NULL || wallet->group_list==NULL || wallet->group_file==NULL){
		return FAILED;
	}
	//we search for a group with the same name
	if(grpsrc(wallet,name,EMPTY)!=NULL){
		return FAILED;
	}
	//we check if we have enough permissions
	if (wallet->current_user->uid != ROOT) {
		return PERM_ERR;
	}
	//we need to generate a new gid, which must be unique, to do so we get the last generade gid+1
	//we get the last created group
	Group* grp=wallet->group_list;
	while(grp->next!=NULL){
		grp=grp->next;
	}
	//now we create a new group
	Group* new_grp=malloc(sizeof(Group));
	new_grp->gid=grp->gid+1;
	//we set the new group as empty
	memset(new_grp->group_members,EMPTY,GROUP_SIZE);
	memcpy(new_grp->group_name,name,strlen(name));
	new_grp->next=NULL;
	new_grp->prev=grp;
	grp->next=new_grp;
	//now we save the group list
	int res= save_groups(wallet->group_file,wallet->group_list,wallet);
	if(res==FAILED || res==PERM_ERR){
		//we revert the changes
		grp->next=NULL;
		free(new_grp);
	}
	return res;
}

//deletes a group
int groupdel(char *name, Wallet *wallet){
	// check if we havethe right parameters
	if(name==NULL || wallet==NULL || wallet->group_list==NULL || wallet->group_file==NULL){
		return FAILED;
	}
	//we check if we have the right permission
	if(wallet->current_user->uid!=ROOT){
		return PERM_ERR;
	}
	//we now search for the group to delete
	Group* grp=grpsrc(wallet,name,EMPTY);
	if(grp==NULL){
		return FAILED;
	}
	//now we delete the user
	Group *prev = NULL, *next = NULL;
	if (grp->prev == NULL) {
		//we delete the first entry in the list
		next = grp->next;
		if(next!=NULL) {
			next->prev = NULL;
		}
	} else {
		if (grp->next == NULL) {
			//we delete the last entry in the list
			prev = grp->prev;
			if(prev!=NULL) {
				prev->next = NULL;
			}
		} else {
			//we delete an entry in between the list
			prev = grp->prev;
			next = grp->next;
			prev->next = next;
			next->prev = prev;
		}
	}
	//we deallocate the memory block
	free(grp);
	//we save the changes in the file
	int res = save_groups(wallet->group_file, wallet->group_list, wallet);
	if(res==FAILED || res==PERM_ERR){
		//we rollback the changes by reloading the list
		delete_group_list(wallet->group_list);
		wallet->group_list=read_groups(wallet->group_file,wallet);
		CHECK_ERR(wallet->group_list==NULL, "users subsystem compromised!")
	}
	return res;
}

//deletes the user list
void delete_user_list(User*lst){
	User* prev;
	while(lst!=NULL){
		prev=lst;
		lst=lst->next;
		free(prev);
	}
}

//deletes the group list
void delete_group_list(Group *lst) {
	Group *prev;
	while (lst != NULL) {
		prev = lst;
		lst = lst->next;
		free(prev);
	}
}

//adds or deletes a user from a group
int gpasswd(char *group, char *user, Wallet *wallet, int type){
	// we check if we are given correct parameters
	if(group==NULL || user==NULL || wallet==NULL || (type!=ADD && type!=REMOVE)){
		return FAILED;
	}
	//check if we have the required permissions
	if(wallet->current_user->uid!=ROOT){
		return  PERM_ERR;
	}
	//we search for the group in which perform the operation
	Group* grp=grpsrc(wallet,group,EMPTY);
	if(grp==NULL){
		return FAILED;
	}
	//we search for the user in which perform the operation
	User *usr = usrsrc(wallet, user, EMPTY);
	if (usr == NULL) {
		return FAILED;
	}
	//we perform the operation
	if(type==ADD){
		//we add a user into a given group
		
		//we search for a empty slot in the group while making sure the user isn't already in the group
		int i;
		for(i=0;grp->group_members[i]!=EMPTY && i<GROUP_SIZE;i++){
			//we check if the user is already in the group
			if(grp->group_members[i]==usr->uid){
				return FAILED;
			}
		}
		//we check if we can fit another user in the group
		if(grp->group_members[i]!=EMPTY){
			return FAILED;
		}
		//we insert the user in the group
		grp->group_members[i]=usr->uid;
	}
	if(type==REMOVE){
		//we remove a user from the given group
		//we search fo the user in the group
		int i,hole=-1;
		for (i = 0; grp->group_members[i] != EMPTY && i < GROUP_SIZE; i++) {
			if (grp->group_members[i] == usr->uid) {
				//if we have found the user we mark its location to be filled with the last user in the list
				hole=i;
			}
		}
		//we check if the user has been found
		if(hole==-1){
			return FAILED;
		}
		//if the user to be removed isn't the last one in the list
		if(hole<i){
			grp->group_members[hole]=grp->group_members[i];
			grp->group_members[i]=EMPTY;
		}else{
			grp->group_members[hole]=EMPTY;
		}
	}
	//now we save the changes into the file
	int res=save_groups(wallet->group_file,wallet->group_list,wallet);
	if (res == FAILED || res == PERM_ERR) {
		//we rollback the changes by reloading the list
		delete_group_list(wallet->group_list);
		wallet->group_list = read_groups(wallet->group_file, wallet);
		CHECK_ERR(wallet->group_list == NULL, "users subsystem compromised!")
	}
	return res;
}

//given a wallet deallocate it along with its content
void destroy_wallet(Wallet *wallet){
	closeFile(wallet->group_file);
	closeFile(wallet->user_file);
	delete_group_list(wallet->group_list);
	delete_user_list(wallet->user_list);
	free(wallet);
}