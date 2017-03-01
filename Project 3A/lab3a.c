#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#define SUPER_BLOCK_OFFSET 1024
#define SUPER_BLOCK_SIZE 1024
#define INODE_BLOCK_POINTER_WIDTH 4
#define GROUP_DESCRIPTOR_TABLE_SIZE 32
#define INODE_SIZE 128

char *fileName;
int imageFd, superFd, groupFd, bitmapFd, inodeFd, directoryFd, indirectFd;
int status, sBuf32;
int numGroups;
uint64_t buf64;
uint32_t buf32;
uint16_t buf16;
uint8_t buf8;
struct super_t *super;
struct group_t *group;
int **validInodeDirectories;
int validInodeDirectoryCount = 0;
int *validInodes;
int validInodeCount = 0;

struct super_t {
	uint16_t magicNumber;
	uint32_t inodeCount, blockCount, blockSize, fragmentSize, blocksPerGroup, inodesPerGroup, fragmentsPerGroup, firstDataBlock;
};

struct group_t {
	uint16_t containedBlockCount, freeBlockCount, freeInodeCount, directoryCount;
	uint32_t inodeBitmapBlock, blockBitmapBlock, inodeTableBlock;
};

void parseArgs(int argc, char **argv){
	if(argc != 2){
		fprintf(stderr, "Error: Please provide a disk image name as the only argument\n");
		exit(EXIT_FAILURE);
	}
	else{
		fileName = malloc( (strlen(argv[1]) + 1) * sizeof(char));
		fileName = argv[1];
		return;
	}
}

void initVars(){
	imageFd = open(fileName, O_RDONLY);
	super = malloc(sizeof(struct super_t));
	group = malloc(sizeof(struct group_t));
}

void parseSuper(){

	// Create csv file
	superFd = creat("super.csv", S_IRWXU);

	// Magic number
	pread(imageFd, &buf16, 2, SUPER_BLOCK_OFFSET + 56);
	super->magicNumber = buf16;
	dprintf(superFd, "%x,", super->magicNumber);

	// inode count
	pread(imageFd, &buf32, 4, SUPER_BLOCK_OFFSET + 0);
	super->inodeCount = buf32;
	dprintf(superFd, "%d,", super->inodeCount);

	// Number of blocks
	pread(imageFd, &buf32, 4, SUPER_BLOCK_OFFSET + 4);
	super->blockCount = buf32;
	dprintf(superFd, "%d,", super->blockCount);

	// Block size
	pread(imageFd, &buf32, 4, SUPER_BLOCK_OFFSET + 24);
	super->blockSize = 1024 << buf32;
	dprintf(superFd, "%d,", super->blockSize);	

	// Fragment size
	pread(imageFd, &sBuf32, 4, SUPER_BLOCK_OFFSET + 28);
	if(sBuf32 > 0){
		super->fragmentSize = 1024 << sBuf32;
	}
	else{
		super->fragmentSize = 1024 >> -sBuf32;
	}
	dprintf(superFd, "%d,", super->fragmentSize);

	// Blocks per group
	pread(imageFd, &buf32, 4, SUPER_BLOCK_OFFSET + 32);
	super->blocksPerGroup = buf32;
	dprintf(superFd, "%d,", super->blocksPerGroup);	

	// inodes per group
	pread(imageFd, &buf32, 4, SUPER_BLOCK_OFFSET + 40);
	super->inodesPerGroup = buf32;
	dprintf(superFd, "%d,", super->inodesPerGroup);	

	// Fragments per group
	pread(imageFd, &buf32, 4, SUPER_BLOCK_OFFSET + 36);
	super->fragmentsPerGroup = buf32;
	dprintf(superFd, "%d,", super->fragmentsPerGroup);

	// First data block
	pread(imageFd, &buf32, 4, SUPER_BLOCK_OFFSET + 20);
	super->firstDataBlock = buf32;
	dprintf(superFd, "%d\n", super->firstDataBlock);

	// Close csv file
	close(superFd);
}

void parseGroup(){

	// Create csv file
	groupFd = creat("group.csv", S_IRWXU);

	// Calculate number of groups
	numGroups = ceil((double)super->blockCount / super->blocksPerGroup);
	double remainder = numGroups - ((double)super->blockCount / super->blocksPerGroup);

	group = malloc(numGroups * sizeof(struct group_t));

	int i;
	for(i = 0; i < numGroups; i++){

		// Number of contained blocks
		if(i != numGroups - 1 || remainder == 0){
			group[i].containedBlockCount = super->blocksPerGroup;
			dprintf(groupFd, "%d,", group[i].containedBlockCount);
		}
		else{
			group[i].containedBlockCount = super->blocksPerGroup * remainder;
			dprintf(groupFd, "%d,", group[i].containedBlockCount);
		}

		// Number of free blocks
		pread(imageFd, &buf16, 2, SUPER_BLOCK_OFFSET + SUPER_BLOCK_SIZE + (i * GROUP_DESCRIPTOR_TABLE_SIZE) + 12);
		group[i].freeBlockCount = buf16;
		dprintf(groupFd, "%d,", group[i].freeBlockCount);

		// Number of free inodes
		pread(imageFd, &buf16, 2, SUPER_BLOCK_OFFSET + SUPER_BLOCK_SIZE + (i * GROUP_DESCRIPTOR_TABLE_SIZE) + 14);
		group[i].freeInodeCount = buf16;
		dprintf(groupFd, "%d,", group[i].freeInodeCount);

		// Number of directories
		pread(imageFd, &buf16, 2, SUPER_BLOCK_OFFSET + SUPER_BLOCK_SIZE + (i * GROUP_DESCRIPTOR_TABLE_SIZE) + 16);
		group[i].directoryCount = buf16;
		dprintf(groupFd, "%d,", group[i].directoryCount);

		// Free inode bitmap block
		pread(imageFd, &buf32, 4, SUPER_BLOCK_OFFSET + SUPER_BLOCK_SIZE + (i * GROUP_DESCRIPTOR_TABLE_SIZE) + 4);
		group[i].inodeBitmapBlock = buf32;
		dprintf(groupFd, "%x,", group[i].inodeBitmapBlock);

		// Free block bitmap block
		pread(imageFd, &buf32, 4, SUPER_BLOCK_OFFSET + SUPER_BLOCK_SIZE + (i * GROUP_DESCRIPTOR_TABLE_SIZE) + 0);
		group[i].blockBitmapBlock = buf32;
		dprintf(groupFd, "%x,", group[i].blockBitmapBlock);

		// Inode table start block
		pread(imageFd, &buf32, 4, SUPER_BLOCK_OFFSET + SUPER_BLOCK_SIZE + (i * GROUP_DESCRIPTOR_TABLE_SIZE) + 8);
		group[i].inodeTableBlock = buf32;
		dprintf(groupFd, "%x\n", group[i].inodeTableBlock);
	}

	// Close csv file
	close(groupFd);
}

void parseBitmap(){

	// Create csv file
	bitmapFd = creat("bitmap.csv", S_IRWXU);

	// Check each group
	int i;
	for(i = 0; i < numGroups; i++){

		// Check each byte in block bitmap
		int j;
		for(j = 0; j < super->blockSize; j++){

			// Read a byte
			pread(imageFd, &buf8, 1, (group[i].blockBitmapBlock * super->blockSize) + j);
			int8_t mask = 1;

			// Analyze the byte
			int k;
			for(k = 1; k <= 8; k++){
				int value = buf8 & mask;
				if(value == 0){
					dprintf(bitmapFd, "%x,", group[i].blockBitmapBlock);
					dprintf(bitmapFd, "%d", j * 8 + k + (i * super->blocksPerGroup));
					dprintf(bitmapFd, "\n");
				}
				mask = mask << 1;
			}
		}

		// Check each byte in inode bitmap
		for(j = 0; j < super->blockSize; j++){

			// Read a byte
			pread(imageFd, &buf8, 1, (group[i].inodeBitmapBlock * super->blockSize) + j);
			int8_t mask = 1;

			// Analyze the byte
			int k;
			for(k = 1; k <= 8; k++){
				int value = buf8 & mask;
				if(value == 0){
					dprintf(bitmapFd, "%x,", group[i].inodeBitmapBlock);
					dprintf(bitmapFd, "%d", j * 8 + k + (i * super->inodesPerGroup));
					dprintf(bitmapFd, "\n");
				}
				mask = mask << 1;
			}
		}
	}

	// Close csv file
	close(bitmapFd);
}

void parseInode(){

	// Create csv file
	inodeFd = creat("inode.csv", S_IRWXU);

	// Allocate space to save valid directory inode information
	validInodeDirectories = malloc(super->inodeCount * sizeof(int*));
	int i;
	for(i = 0; i < super->inodeCount; i++){
		validInodeDirectories[i] = malloc(2 * sizeof(int));
	} 

	// Allocate space to save valid inode information
	validInodes = malloc(super->inodeCount * sizeof(int));

	// Check each group
	for(i = 0; i < numGroups; i++){

		// Check each byte in the inode bitmap
		int j;
		for(j = 0; j < super->blockSize; j++){

			// Read a byte
			pread(imageFd, &buf8, 1, (group[i].inodeBitmapBlock * super->blockSize) + j);
			int8_t mask = 1;

			// Analyze the byte
			int k;
			for(k = 1; k <= 8; k++){
				int value = buf8 & mask;
				if(value != 0 && (j * 8 + k) <= super->inodesPerGroup){

					// Get inode number
					int inodeNumber = j * 8 + k + (i * super->inodesPerGroup);
					dprintf(inodeFd, "%d,", inodeNumber);

					// Get file type
					pread(imageFd, &buf16, 2, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 0);
					validInodes[validInodeCount] = group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE;
					validInodeCount++;

					// Check for regular files
					if(buf16 & 0x8000){
						dprintf(inodeFd, "f,");
					}

					// Check for symbolic files
					else if(buf16 & 0xA000){
						dprintf(inodeFd, "s,");
					}

					// Check for directories
					else if(buf16 & 0x4000){
						validInodeDirectories[validInodeDirectoryCount][0] = group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE;
						validInodeDirectories[validInodeDirectoryCount][1] = inodeNumber;
						validInodeDirectoryCount++;
						dprintf(inodeFd, "d,");
					}

					// Check for other files
					else{
						dprintf(inodeFd, "?,");
					}

					// Get mode
					dprintf(inodeFd, "%o,", buf16);

					// Get owner
					uint32_t uid;
					pread(imageFd, &buf32, 2, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 2);
					uid = buf32;
					pread(imageFd, &buf32, 2, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 116 + 4);
					uid = uid | (buf32 << 16);
					dprintf(inodeFd, "%d,", uid);

					// Get group
					uint32_t gid;
					pread(imageFd, &buf32, 2, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 24);
					gid = buf32;
					pread(imageFd, &buf32, 2, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 116 + 6);
					gid = gid | (buf32 << 16);
					dprintf(inodeFd, "%d,", gid);

					// Get link count
					pread(imageFd, &buf16, 2, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 26);
					dprintf(inodeFd, "%d,", buf16);

					// Get creation time
					pread(imageFd, &buf32, 4, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 12);
					dprintf(inodeFd, "%x,", buf32);

					// Get modification time
					pread(imageFd, &buf32, 4, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 16);
					dprintf(inodeFd, "%x,", buf32);

					// Get access time
					pread(imageFd, &buf32, 4, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 8);
					dprintf(inodeFd, "%x,", buf32);

					// Get file size
					uint64_t size;
					pread(imageFd, &buf64, 4, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 4);
					size = buf64;					
					pread(imageFd, &buf64, 4, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 108);
					size = size | (buf64 << 32);
					dprintf(inodeFd, "%ld,", size);

					// Get number of blocks
					pread(imageFd, &buf32, 4, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 28);
					dprintf(inodeFd, "%d,", buf32/(super->blockSize/512));

					// Get block pointers
					int x;
					for(x = 0; x < 15; x++){
						pread(imageFd, &buf32, 4, group[i].inodeTableBlock * super->blockSize + (j * 8 + k - 1) * INODE_SIZE + 40 + (x * 4));
						if(x != 14){
							dprintf(inodeFd, "%x,", buf32);
						}
						else{
							dprintf(inodeFd, "%x", buf32);
						}
					}

					dprintf(inodeFd, "\n");

					buf16 = 0;
					buf32 = 0;
					buf64 = 0;
				}
				mask = mask << 1;
			}
		}
	}

	// Close csv file
	close(inodeFd);
}

void parseDirectory(){

	// Create csv file
	directoryFd = creat("directory.csv", S_IRWXU);

	// Loop through valid directory inodes
	int i;
	for(i = 0; i < validInodeDirectoryCount; i++){

		// Direct blocks
		int entryNum = 0;	
		int j;
		for(j = 0; j < 12; j++){	
			pread(imageFd, &buf32, 4, (validInodeDirectories[i][0] + 40 + (j * 4)));
			uint32_t dataOffset = buf32;
			if(dataOffset != 0){
				int currentOffset = super->blockSize * dataOffset;
				while(currentOffset < super->blockSize * dataOffset + super->blockSize){

					// Check name length
					pread(imageFd, &buf8, 1, currentOffset + 6);
					int nameLen = buf8;

					// Check inode number
					pread(imageFd, &buf32, 4, currentOffset);
					int inodeNumber = buf32;

					// Check entry length
					pread(imageFd, &buf16, 2, currentOffset + 4);
					int entryLength = buf16;

					if(inodeNumber == 0){
						currentOffset += entryLength;
						entryNum++;
						continue;
					}

					// Get parent inode number
					dprintf(directoryFd, "%d,", validInodeDirectories[i][1]);

					// Get entry number
					dprintf(directoryFd, "%d,", entryNum);
					entryNum++;

					// Get entry length
					dprintf(directoryFd, "%d,", entryLength);

					// Get name length
					dprintf(directoryFd, "%d,", nameLen);

					// Get inode number
					dprintf(directoryFd, "%d,", inodeNumber);

					// Get name
					char charBuf;
					dprintf(directoryFd, "\"");
					int k;
					for(k = 0; k < nameLen; k++){
						pread(imageFd, &charBuf, 1, currentOffset + 8 + k);
						dprintf(directoryFd, "%c", charBuf);
					}
					dprintf(directoryFd, "\"\n");

					currentOffset += entryLength;
				}
			}
		}

		// Indirect blocks
		pread(imageFd, &buf32, 4, (validInodeDirectories[i][0] + 40 + (12 * 4)));
		uint32_t dataOffset = buf32;
		if(dataOffset != 0){
			j = 0;
			for(j = 0; j < super->blockSize / 4; j++){
				int currentOffset = super->blockSize * dataOffset + (j * 4);
				pread(imageFd, &buf32, 4, currentOffset);
				int blockNum2 = buf32;
				if(blockNum2 != 0){
					currentOffset = blockNum2 * super->blockSize;
					while(currentOffset < blockNum2 * super->blockSize + super->blockSize){

						// Check name length
						pread(imageFd, &buf8, 1, currentOffset + 6);
						int nameLen = buf8;

						// Check inode number
						pread(imageFd, &buf32, 4, currentOffset);
						int inodeNumber = buf32;

						// Check entry length
						pread(imageFd, &buf16, 2, currentOffset + 4);
						int entryLength = buf16;

						if(inodeNumber == 0){
							currentOffset += entryLength;
							entryNum++;
							continue;
						}

						// Get parent inode number
						dprintf(directoryFd, "%d,", validInodeDirectories[i][1]);

						// Get entry number
						dprintf(directoryFd, "%d,", entryNum);
						entryNum++;

						// Get entry length
						dprintf(directoryFd, "%d,", entryLength);

						// Get name length
						dprintf(directoryFd, "%d,", nameLen);

						// Get inode number
						dprintf(directoryFd, "%d,", inodeNumber);

						// Get name
						char charBuf;
						dprintf(directoryFd, "\"");
						int k;
						for(k = 0; k < nameLen; k++){
							pread(imageFd, &charBuf, 1, currentOffset + 8 + k);
							dprintf(directoryFd, "%c", charBuf);
						}
						dprintf(directoryFd, "\"\n");

						currentOffset += entryLength;
					}
				}
			}
		}

		// Double indirect blocks
		pread(imageFd, &buf32, 4, (validInodeDirectories[i][0] + 40 + (13 * 4)));
		dataOffset = buf32;
		if(dataOffset != 0){
			j = 0;
			for(j = 0; j < super->blockSize / 4; j++){
				int currentOffset = super->blockSize * dataOffset + (j * 4);
				pread(imageFd, &buf32, 4, currentOffset);
				int blockNum2 = buf32;
				if(blockNum2 != 0){
					int k;
					for(k = 0; k < super->blockSize / 4; k++){
						pread(imageFd, &buf32, 4, blockNum2 * super->blockSize + (k * 4));
						int blockNum3 = buf32;
						if(blockNum3 != 0){
							currentOffset = blockNum3 * super->blockSize;
							while(currentOffset < blockNum3 * super->blockSize + super->blockSize){

								// Check name length
								pread(imageFd, &buf8, 1, currentOffset + 6);
								int nameLen = buf8;

								// Check inode number
								pread(imageFd, &buf32, 4, currentOffset);
								int inodeNumber = buf32;

								// Check entry length
								pread(imageFd, &buf16, 2, currentOffset + 4);
								int entryLength = buf16;

								if(inodeNumber == 0){
									currentOffset += entryLength;
									entryNum++;
									continue;
								}

								// Get parent inode number
								dprintf(directoryFd, "%d,", validInodeDirectories[i][1]);

								// Get entry number
								dprintf(directoryFd, "%d,", entryNum);
								entryNum++;

								// Get entry length
								dprintf(directoryFd, "%d,", entryLength);

								// Get name length
								dprintf(directoryFd, "%d,", nameLen);

								// Get inode number
								dprintf(directoryFd, "%d,", inodeNumber);

								// Get name
								char charBuf;
								dprintf(directoryFd, "\"");
								int k;
								for(k = 0; k < nameLen; k++){
									pread(imageFd, &charBuf, 1, currentOffset + 8 + k);
									dprintf(directoryFd, "%c", charBuf);
								}
								dprintf(directoryFd, "\"\n");

								currentOffset += entryLength;
							}
						}
					}
				}
			}
		}

		// Triple indirect blocks
		pread(imageFd, &buf32, 4, (validInodeDirectories[i][0] + 40 + (14 * 4)));
		dataOffset = buf32;
		if(dataOffset != 0){
			j = 0;
			for(j = 0; j < super->blockSize / 4; j++){
				int currentOffset = super->blockSize * dataOffset + (j * 4);
				pread(imageFd, &buf32, 4, currentOffset);
				int blockNum2 = buf32;
				if(blockNum2 != 0){
					int k;
					for(k = 0; k < super->blockSize / 4; k++){
						pread(imageFd, &buf32, 4, blockNum2 * super->blockSize + (k * 4));
						int blockNum3 = buf32;
						if(blockNum3 != 0){
							int x;
							for(x = 0; x < super->blockSize / 4; x++){
								pread(imageFd, &buf32, 4, blockNum3 * super->blockSize + (x * 4));
								int blockNum4 = buf32;
								if(blockNum4 != 0){
									currentOffset = blockNum4 * super->blockSize;
									while(currentOffset < blockNum4 * super->blockSize + super->blockSize){

										// Check name length
										pread(imageFd, &buf8, 1, currentOffset + 6);
										int nameLen = buf8;

										// Check inode number
										pread(imageFd, &buf32, 4, currentOffset);
										int inodeNumber = buf32;

										// Check entry length
										pread(imageFd, &buf16, 2, currentOffset + 4);
										int entryLength = buf16;

										if(inodeNumber == 0){
											currentOffset += entryLength;
											entryNum++;
											continue;
										}

										// Get parent inode number
										dprintf(directoryFd, "%d,", validInodeDirectories[i][1]);

										// Get entry number
										dprintf(directoryFd, "%d,", entryNum);
										entryNum++;

										// Get entry length
										dprintf(directoryFd, "%d,", entryLength);

										// Get name length
										dprintf(directoryFd, "%d,", nameLen);

										// Get inode number
										dprintf(directoryFd, "%d,", inodeNumber);

										// Get name
										char charBuf;
										dprintf(directoryFd, "\"");
										int k;
										for(k = 0; k < nameLen; k++){
											pread(imageFd, &charBuf, 1, currentOffset + 8 + k);
											dprintf(directoryFd, "%c", charBuf);
										}
										dprintf(directoryFd, "\"\n");

										currentOffset += entryLength;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	// Close csv file
	close(directoryFd);
}

void parseIndirect(){

	// Create csv file
	indirectFd = creat("indirect.csv", S_IRWXU);

	int entryNumber = 0;

	// Loop through valid inodes
	int i;
	for(i = 0; i < validInodeCount; i++){

		// Single indirect
		entryNumber = 0;
		pread(imageFd, &buf32, 4, validInodes[i] + 40 + (12 * 4));
		int blockNumber = buf32;
		int j;
		for(j = 0; j < super->blockSize / 4; j++){
			pread(imageFd, &buf32, 4, blockNumber * super->blockSize + (j * 4));
			int blockNumber2 = buf32;
			if(blockNumber2 != 0){
				dprintf(indirectFd, "%x,", blockNumber);

				dprintf(indirectFd, "%d,", entryNumber);
				entryNumber++;

				dprintf(indirectFd, "%x\n", blockNumber2);
			}
		}

		// Double indirect
		entryNumber = 0;
		pread(imageFd, &buf32, 4, validInodes[i] + 40 + (13 * 4));
		blockNumber = buf32;
		for(j = 0; j < super->blockSize / 4; j++){
			pread(imageFd, &buf32, 4, blockNumber * super->blockSize + (j * 4));
			int blockNumber2 = buf32;
			if(blockNumber2 != 0){
				dprintf(indirectFd, "%x,", blockNumber);

				dprintf(indirectFd, "%d,", entryNumber);
				entryNumber++;

				dprintf(indirectFd, "%x\n", blockNumber2);
			}
		}

		entryNumber = 0;
		for(j = 0; j < super->blockSize / 4; j++){
			pread(imageFd, &buf32, 4, blockNumber * super->blockSize + (j * 4));
			int blockNumber2 = buf32;
			if(blockNumber2 != 0){
				entryNumber = 0;
				int k;
				for(k = 0; k < super->blockSize / 4; k++){
					pread(imageFd, &buf32, 4, blockNumber2 * super->blockSize + (k * 4));
					int blockNumber3 = buf32;
					if(blockNumber3 != 0){
						dprintf(indirectFd, "%x,", blockNumber2);

						dprintf(indirectFd, "%d,", entryNumber);
						entryNumber++;

						dprintf(indirectFd, "%x\n", blockNumber3);
					}
				}
			}
		}

		// Triple indirect
		entryNumber = 0;
		pread(imageFd, &buf32, 4, validInodes[i] + 40 + (14 * 4));
		blockNumber = buf32;
		for(j = 0; j < super->blockSize / 4; j++){
			pread(imageFd, &buf32, 4, blockNumber * super->blockSize + (j * 4));
			int blockNumber2 = buf32;
			if(blockNumber2 != 0){
				dprintf(indirectFd, "%x,", blockNumber);

				dprintf(indirectFd, "%d,", entryNumber);
				entryNumber++;

				dprintf(indirectFd, "%x\n", blockNumber2);
			}
		}

		entryNumber = 0;
		for(j = 0; j < super->blockSize / 4; j++){
			pread(imageFd, &buf32, 4, blockNumber * super->blockSize + (j * 4));
			int blockNumber2 = buf32;
			if(blockNumber2 != 0){
				entryNumber = 0;
				int k;
				for(k = 0; k < super->blockSize / 4; k++){
					pread(imageFd, &buf32, 4, blockNumber2 * super->blockSize + (k * 4));
					int blockNumber3 = buf32;
					if(blockNumber3 != 0){
						dprintf(indirectFd, "%x,", blockNumber2);

						dprintf(indirectFd, "%d,", entryNumber);
						entryNumber++;

						dprintf(indirectFd, "%x\n", blockNumber3);
					}
				}
			}
		}

		entryNumber = 0;
		for(j = 0; j < super->blockSize / 4; j++){
			pread(imageFd, &buf32, 4, blockNumber * super->blockSize + (j * 4));
			int blockNumber2 = buf32;
			if(blockNumber2 != 0){
				entryNumber = 0;
				int k;
				for(k = 0; k < super->blockSize / 4; k++){
					pread(imageFd, &buf32, 4, blockNumber2 * super->blockSize + (k * 4));
					int blockNumber3 = buf32;
					if(blockNumber3 != 0){
						entryNumber = 0;
						int x;
						for(x = 0; x < super->blockSize / 4; x++){
							pread(imageFd, &buf32, 4, blockNumber3 * super->blockSize + (x * 4));
							int blockNumber4 = buf32;
							if(blockNumber4 != 0){
								dprintf(indirectFd, "%x,", blockNumber3);

								dprintf(indirectFd, "%d,", entryNumber);
								entryNumber++;

								dprintf(indirectFd, "%x\n", blockNumber4);
							}
						}
					}
				}
			}
		}
	}

	// Close csv file
	close(indirectFd);
}

int main(int argc, char **argv){
	parseArgs(argc, argv);
	initVars();
	parseSuper();
	parseGroup();
	parseBitmap();
	parseInode();
	parseDirectory();
	parseIndirect();
	close(imageFd);
	exit(EXIT_SUCCESS);
}
