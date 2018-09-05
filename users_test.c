#include "users.h"
#include "simplefs_apis.h"

int main(void) {
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
	DirectoryHandle *dh = init(fs, disk, root, root);
	
	//we initialize the wallet
	printf("create wallet\n");
	Wallet *wallet = initialize_wallet(dh);
	CHECK_ERR(wallet == NULL, "failed wallet creation")
	//we now add two users
	char usr1[] = "user1", usr2[] = "user2", usr3[] = "user3";
	printf("add user1\n");
	res = useradd(usr1, wallet);
	printf("result:%d\n", res);
	printf("add user2\n");
	res = useradd(usr2, wallet);
	printf("result:%d\n", res);
	printf("add user1 in user2 group\n");
	res = gpasswd(usr2, usr1, wallet, ADD);
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
	printf("now we destroy and reload the wallet and perform the same operations\n");
	wallet = initialize_wallet(dh);
	printf("add user1\n");
	res = useradd(usr1, wallet);
	printf("result:%d\n", res);
	printf("add user3\n");
	res = useradd(usr3, wallet);
	printf("result:%d\n", res);
	printf("add user1 in user2 group\n");
	res = gpasswd(usr3, usr1, wallet, ADD);
	printf("result:%d\n", res);
	printf("add user3 in user1 group\n");
	res = gpasswd(usr1, usr3, wallet, ADD);
	printf("result:%d\n", res);
	printf("remove user3 in user1 group\n");
	res = gpasswd(usr1, usr3, wallet, REMOVE);
	printf("result:%d\n", res);
	printf("remove user1\n");
	res = userdel(usr1, wallet);
	printf("result:%d\n", res);
	printf("remove user3\n");
	res = userdel(usr3, wallet);
	printf("result:%d\n", res);
	destroy_wallet(wallet);
}