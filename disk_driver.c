#include "disk_driver.h"

void DiskDriver_init(DiskDriver *disk, const char *filename, int num_blocks) {
	int res, fd;
	//checking if the file exists or if we need to format the drive
	res = access(filename, F_OK);
	CHECK_ERR(res == FAILED && errno != ENOENT, "Can't test if the file exists");
	CHECK_ERR(res != FAILED && res != 0, "Can't test if the file exists"); // TO VERIFY
	
	// opening the file and creating it if necessary
	fd = open(filename, O_CREAT | O_RDWR, 0666);
	//checking if the open was successful
	CHECK_ERR(fd == FAILED, "error opening the file");
	
	//calculating the bitmap size (rounded up)
	int bitmap_blocks = (num_blocks + 7) / 8;
	//rounded up block occupation of DiskHeader and bitmap
	size_t occupation = (sizeof(DiskHeader) + bitmap_blocks + sizeof(BitMap) + BLOCK_SIZE - 1) / BLOCK_SIZE;
	//now we need to prepare the file to be mmapped because it needs to have the same dimension of the mmapped array
	//to do so we need to write something (/0) to the last byte of the file (num_blocks*BLOCK_SIZE)
	//we have repositioned the file pointer to the last byte
	
	res = (int) lseek(fd, (occupation * BLOCK_SIZE) - 1, SEEK_SET);
	CHECK_ERR(res == FAILED, "can't reposition pointer file");
	//now we write something so the file will actually have this dimension
	res = (int) write(fd, "\0", 1);
	CHECK_ERR(res == FAILED, "can't write into file");
	
	//mmap the file
	void *map = mmap(0, occupation * BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	CHECK_ERR(map == MAP_FAILED, "error mapping the file");
	
	//creating and initializing the disk header
	DiskHeader *d = (DiskHeader *) map;
	d->num_blocks = num_blocks;
	d->bitmap_blocks = (bitmap_blocks + sizeof(BitMap) + BLOCK_SIZE - 1) / BLOCK_SIZE;
	d->bitmap_entries = bitmap_blocks;
	d->free_blocks = (int) (num_blocks - occupation);
	d->first_free_block = (int) occupation;
	
	//initializing the bitmap
	BitMap_init((BitMap *) (map + sizeof(DiskHeader)), bitmap_blocks, num_blocks, (int) occupation);
	
	//populating the disk driver
	disk->header = (DiskHeader *) map;
	disk->bmap = (BitMap *) (map + sizeof(DiskHeader));
	disk->fd = fd;
}

// Reads the first block of the disk file
// returns -1 if the block is free according to the bitmap
// 0 otherwise
int DiskDriver_readFirstBlock(DiskDriver *disk, void *dest, int block_num) {
	// Check if the parameters passed are valid
	if (disk == NULL) return FAILED;
	if (dest == NULL) return FAILED;
	if (block_num < 0) return FAILED;
	
	// Set the position to read from
	int res;
	res = (int) lseek(disk->fd, block_num * BLOCK_SIZE, SEEK_SET);
	if (res == -1) return FAILED;
	
	//now we read something so the file will actually have this dimension
	res = (int) read(disk->fd, dest, BLOCK_SIZE);
	
	// If the read was interrupted by an interrupt, then reading again
	while (res == -1 && errno == EINTR) {
		res = (int) lseek(disk->fd, block_num * BLOCK_SIZE, SEEK_SET);
		if (res == -1) return FAILED;
		res = (int) read(disk->fd, dest, BLOCK_SIZE);
	}
	
	if (res != BLOCK_SIZE) return FAILED;
	
	return SUCCESS;
}

//assuming that the file is already initialized loads the disk
int DiskDriver_load(DiskDriver *disk, const char *filename) {
	int res, fd; //1= file already exists 0=file doesn't exists yet
	//checking if the file exists or if we need to format the drive
	res = access(filename, F_OK);
	if (res == FAILED && errno == ENOENT) {
		return FAILED;
	} else {
		CHECK_ERR(res == FAILED, "Can't test if the file exists");\

	}
	
	// opening the file and creating it if necessary
	fd = open(filename, O_RDWR, 0666);
	//checking if the open was successful
	CHECK_ERR(fd == FAILED, "error opening the file");
	
	// Copio fd del file disco nella struttura
	disk->fd = fd;
	
	//reading the first block of the disk
	void *block = malloc(BLOCK_SIZE);
	res = DiskDriver_readFirstBlock(disk, block, 0);
	CHECK_ERR(res == FAILED, "CAN't read the first block of disk");
	
	//calculating the bitmap size (rounded up)
	int bitmap_blocks = (((DiskHeader *) block)->num_blocks + 7) / 8;
	//rounded up block occupation of DiskHeader and bitmap
	size_t occupation = (sizeof(DiskHeader) + bitmap_blocks + sizeof(BitMap) + BLOCK_SIZE - 1) / BLOCK_SIZE;
	
	//now we can dealloate the read block
	free(block);
	//mmap the file
	void *map = mmap(0, occupation * BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	CHECK_ERR(map == MAP_FAILED && errno != SIGBUS, "error mapping the file");
	
	if (map == MAP_FAILED) {
		return FAILED;
	}
	
	//populating the disk driver
	disk->header = (DiskHeader *) map;
	disk->bmap = (BitMap *) (map + sizeof(DiskHeader));
	disk->bmap->entries = (uint8_t *) disk->bmap + sizeof(BitMap);
	
	return SUCCESS;
}

// Reads the block in position block_num
// returns -1 if the block is free according to the bitmap
// 0 otherwise
int DiskDriver_readBlock(DiskDriver *disk, void *dest, int block_num) {
	// Check if the parameters passed are valid
	if (disk == NULL) return FAILED;
	if (dest == NULL) return FAILED;
	if (block_num < 0 || block_num > disk->header->num_blocks) return FAILED;
	
	// Set the position to read from
	int res;
	res = (int) lseek(disk->fd, block_num * BLOCK_SIZE, SEEK_SET);
	if (res == -1) return FAILED;
	
	//now we read something so the file will actually have this dimension
	res = (int) read(disk->fd, dest, BLOCK_SIZE);
	
	// If the read was interrupted by an interrupt, then reading again
	while (res == -1 && errno == EINTR) {
		res = (int) lseek(disk->fd, block_num * BLOCK_SIZE, SEEK_SET);
		if (res == -1) return FAILED;
		res = (int) read(disk->fd, dest, BLOCK_SIZE);
	}
	
	if (res != BLOCK_SIZE) return FAILED;
	
	return SUCCESS;
}

// writes a block in position block_num, and alters the bitmap accordingly
// returns -1 if operation not possible
int DiskDriver_writeBlock(DiskDriver *disk, void *src, int block_num) {
	// Check if the parameters passed are valid
	if (disk == NULL) return FAILED;
	if (src == NULL) return FAILED;
	if (block_num < 0 || block_num > disk->header->num_blocks) return FAILED;
	
	// Change status and check if the status of the bitmap block has been changed correctly
	int sucStatus = BitMap_set(disk->bmap, block_num, BLOCK_USED);
	if (sucStatus != BLOCK_USED) return FAILED;
	
	// Set the position to read from
	int res;
	res = (int) lseek(disk->fd, block_num * BLOCK_SIZE, SEEK_SET);
	if (res == -1) return FAILED;
	
	//now we write something so the file will actually have this dimension
	res = (int) write(disk->fd, src, BLOCK_SIZE);
	
	// If the write was interrupted by an interrupt, then writing again
	while (res == -1 && errno == EINTR) {
		res = (int) lseek(disk->fd, block_num * BLOCK_SIZE, SEEK_SET);
		if (res == -1) return FAILED;
		res = (int) write(disk->fd, src, BLOCK_SIZE);
	}
	
	if (res != BLOCK_SIZE) return FAILED;
	
	return SUCCESS;
}

// frees a block in position block_num, and alters the bitmap accordingly
// returns -1 if operation not possible
int DiskDriver_freeBlock(DiskDriver *disk, int block_num) {
	// Check if the parameters passed are valid
	if (disk == NULL) return FAILED;
	if (block_num < 0 || block_num > disk->header->num_blocks) return FAILED;
	
	int status = BitMap_test(disk->bmap, block_num);
	if (status == BLOCK_FREE) return SUCCESS;
	
	// Change status and check if the status of the bitmap block
	// has been changed correctly
	int sucStatus = BitMap_set(disk->bmap, block_num, BLOCK_FREE);
	if (sucStatus != BLOCK_FREE) return FAILED;
	
	// Inc num of free block
	disk->header->free_blocks++;
	if (disk->header->first_free_block == -1) {
		disk->header->first_free_block = block_num;
	}
	return SUCCESS;
}

// returns the first free block in the disk from position (checking the bitmap)
int DiskDriver_getFreeBlock(DiskDriver *disk, int start) {
	// Check if the parameter passed is valid
	if (disk == NULL) return FAILED;
	if (start < 0 || start > disk->header->num_blocks) return FAILED;
	
	// Check if the block has not been used
	int new_block, retStat;
	new_block = disk->header->first_free_block;
	//if there aren't free blocks
	if (new_block == FAILED) {
		new_block = BitMap_get(disk->bmap, 0, BLOCK_FREE);
		if (new_block == FAILED) {
			return FAILED;
		}
	}
	retStat = BitMap_test(disk->bmap, new_block);
	
	// If new_block is already used then find a new_block free
	if (retStat == BLOCK_USED) {
		new_block = BitMap_get(disk->bmap, start, BLOCK_FREE);
	}
	
	
	retStat = BitMap_get(disk->bmap, new_block + 1, BLOCK_FREE);
	if (retStat == FAILED) {
		retStat = BitMap_get(disk->bmap, 0, BLOCK_FREE);
	}
	disk->header->first_free_block = retStat;
	// Dec num of free block
	disk->header->free_blocks--;
	
	return new_block;
}

// writes the data (flushing the mmaps)
int DiskDriver_flush(DiskDriver *disk) {
	// Check if the parameter passed is valid
	if (disk == NULL) return FAILED;
	
	//calculating the bitmap size (rounded up)
	int bitmap_blocks = (disk->header->num_blocks + 7) / 8;
	//rounded up block occupation of DiskHeader and bitmap
	size_t occupation = (sizeof(DiskHeader) + bitmap_blocks + sizeof(BitMap) + BLOCK_SIZE - 1) / BLOCK_SIZE;
	
	// Sync between map and file and check possible error
	int res = msync(disk->header, occupation * BLOCK_SIZE, MS_SYNC);
	if (res == -1) return FAILED;
	
	res = fsync(disk->fd);
	if (res == -1) return FAILED;
	
	return SUCCESS;
}

void DiskDriver_shutdown(DiskDriver *disk) {
	int res;
	
	//calculating the bitmap size (rounded up)
	int bitmap_blocks = (disk->header->num_blocks + 7) / 8;
	//rounded up block occupation of DiskHeader and bitmap
	size_t occupation = (sizeof(DiskHeader) + bitmap_blocks + sizeof(BitMap) + BLOCK_SIZE - 1) / BLOCK_SIZE;
	
	//flushing changes
	res = DiskDriver_flush(disk);
	CHECK_ERR(res == FAILED, "can't sync changes");
	
	//unmapping the mappend memory
	res = munmap(disk->header, occupation * BLOCK_SIZE);
	CHECK_ERR(res == -1, "can't unmap the file");
	
	//closing the file
	close(disk->fd);
	CHECK_ERR(res == -1, "can't close the file");
}
