#include "users.h"
#include "simplefs.h"
#include "simplefs_apis.h"

int main(void){
	//we create and load the disk
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
	DirectoryHandle *dh = init(fs, disk, root);
	
	//we initialize the wallet
	printf("create wallet\n");
	Wallet* wallet=initialize_wallet(dh);
	CHECK_ERR(wallet==NULL,"failed wallet creation")
	//we now add two users
	char usr1[]="user1", usr2[]="user2";
	printf("add user1\n");
	res=useradd(usr1,wallet);
	printf("result:%d\n",res);
	printf("add user2\n");
	res = useradd(usr2, wallet);
	printf("result:%d\n", res);
	printf("add user1 in user2 group\n");
	res = gpasswd(usr2,usr1, wallet,ADD);
	printf("result:%d\n", res);
	printf("add user2 in user1 group\n");
	res = gpasswd(usr1, usr2, wallet, ADD);
	printf("result:%d\n", res);
	printf("remove user2 in user1 group\n");
	res = gpasswd(usr1, usr2, wallet, REMOVE);
	printf("result:%d\n", res);
	printf("remove user1\n");
	res = userdel(usr1, wallet);
	printf("result:%d\n", res);
	printf("remove user2\n");
	res = userdel(usr2, wallet);
	printf("result:%d\n", res);
	destroy_wallet(wallet);
}