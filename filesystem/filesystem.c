/**************************************************
			Simple Filesystem Simulator
					using
				   i-nodes

				Author: Jiri Besta
***************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#define FS_NAME_MAX_L 11	// Maximum length of name (with extension)
#define BUFF_SIZE 256		// Buffer size for input commands

#define FNF "FILE NOT FOUND\n"
#define PNF "PATH NOT FOUND\n"
#define EXIST "EXIST\n"
#define NE "NOT EMPTY\n"
#define F "-FILE\n"
#define D "+DIRECTORY\n"
#define OK "OK\n"
#define CCF "CANNOT CREATE FILE\n"

struct superblock {
 //   char signature[9];              //login autora FS
 //   char volume_descriptor[251];    //popis vygenerovaného FS
    int32_t disk_size;              //celkova velikost VFS
    int32_t cluster_size;           //velikost clusteru
    int32_t cluster_count;          //pocet clusteru
    int32_t inode_block_count;		//pocet i-nodu
    int32_t data_block_count;		//pocet datovych bloku
    int32_t bitmap_start_address;   //adresa pocatku bitmapy datových bloku
    int32_t inode_start_address;    //adresa pocatku  i-uzlu
    int32_t data_start_address;     //adresa pocatku datovych bloku  
};


struct inode {
    int32_t nodeid;                 //ID i-uzlu, pokud ID = ID_ITEM_FREE, je polozka volna
    int8_t isDirectory;               // 0 = soubor, 1 = adresar
 //   int8_t references;              //pocet odkazu na i-uzel, používá se pro hardlinky
    int32_t file_size;              //velikost souboru v bytech
    int32_t direct1;                // 1. prímý odkaz na datové bloky
    int32_t direct2;                // 2. prímý odkaz na datové bloky
    int32_t direct3;                // 3. prímý odkaz na datové bloky
    int32_t direct4;                // 4. prímý odkaz na datové bloky
    int32_t direct5;                // 5. prímý odkaz na datové bloky
    int32_t indirect1;              // 1. neprímý odkaz (odkaz - datové bloky)
    int32_t indirect2;              // 2. neprímý odkaz (odkaz - odkaz - datové bloky)
};


struct directory_item {
    int32_t inode;                   // inode odpovídající souboru
    char item_name[12];              //8+3 + /0 C/C++ ukoncovaci string znak
};

const long DEFAULT_FS_SIZE = 102400000; // Default size of the filesystem in bytes (102,4MB)
const int32_t INODE_SIZE = 37;			// Size of the i-node in bytes
const int32_t CLUSTER_SIZE = 1024; 		// Size of the cluster in bytes
const int32_t ID_ITEM_FREE = 0;
const char *DELIM = " ,\n"; 

FILE *fs;						// File with filesystem
struct superblock *sb;			// Superblock
int8_t *bitmap = NULL;			// Bitmap of data blocks, 0 = free	1 = full
struct inode *inodes = NULL;	// Array of i-nodes

void defrag(char *cmd) {
	
}

/* 	Format existing filesystem or create a new one with specific size

	@param bytes ... size of the filesystem in bytes
*/
void format(long bytes) {
	int i, bitmap_block_count;
	
	printf("Formating...\n");
	
	// Prepare superblock
	sb = (struct superblock *)malloc(sizeof(struct superblock));
	if (!sb) {
		printf(CCF);
		return;
	}
	
	sb->cluster_size = CLUSTER_SIZE;
	sb->cluster_count = bytes / CLUSTER_SIZE; // Count of all clusters
	sb->disk_size = sb->cluster_count * CLUSTER_SIZE; // Exact size of the filesystem in bytes
	sb->inode_block_count = sb->cluster_count / 10; // Count of blocks for i-nodes, 10% of all blocks
	sb->bitmap_start_address = CLUSTER_SIZE; // Bitmap is located after superblock -> offset = CLUSTER_SIZE (1024)
	
	bitmap_block_count = ceil((sb->cluster_count - sb->inode_block_count - 1) / (float)CLUSTER_SIZE);
	sb->data_block_count = sb->cluster_count - 1 - bitmap_block_count - sb->inode_block_count;
	
	sb->inode_start_address = sb->bitmap_start_address + CLUSTER_SIZE * bitmap_block_count;
	sb->data_start_address = sb->inode_start_address + CLUSTER_SIZE * sb->inode_block_count;
	
	printf("Size: %d\nCount of clusters: %d\nCount of bitmap blocks: %d\nCount of i-node blocks: %d\nCount of data blocks: %d\nAddress of bitmap: %d\nAddress of i-nodes: %d\nAddress of data: %d\n", 
	sb->disk_size, sb->cluster_count, bitmap_block_count, sb->inode_block_count, sb->data_block_count, sb->bitmap_start_address, sb->inode_start_address, sb->data_start_address);
	
	bitmap = (int8_t *)malloc(sb->data_block_count);
	inodes = (struct inode *)malloc(sizeof(struct inode) * ((sb->inode_block_count * CLUSTER_SIZE) / INODE_SIZE));
	if (!bitmap || !inodes) {
		printf(CCF);
		return;
	}
		
	for (int i = 0; i < sb->disk_size; i++) {
		fputc('0', fs);
	}
	
	fflush(fs);
	printf(OK);
}

void load(char *file) {
	
}

void outcp(char *files) {
	
}

void incp(char *files) {
	
}

void info(char *file) {
	
}

void pwd() {

}

void cd(char *dir) {
	
}

void cat(char *file) {
	
}

void ls(char *dir) {
	
}

void myrmdir(char *dir) {
	
}

void mymkdir(char *dir) {
	
}

void rm(char *file) {
	
}

void mv(char *files) {
	
	
}

void cp(char *files) {
	
	
	printf("%s\n", files);
}

// Process user commands
int run() {
	short exit = 0;
	char buffer[BUFF_SIZE] = {0};
	char tmp_buff[BUFF_SIZE];
	char *cmd;
	
	printf("Filesystem is running...\n");
	
	do {
		// Load user command into buffer
		memset(buffer, 0, BUFF_SIZE);
		fgets(buffer, BUFF_SIZE, stdin);
		
		strncpy(tmp_buff, buffer, BUFF_SIZE);
		cmd = strtok(tmp_buff, DELIM);

//		printf("Length: %d, %s\n", strlen(cmd), cmd);
		
		if (strcmp("cp", cmd) == 0) {
			cp(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("mv", cmd) == 0) {
			mv(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("rm", cmd) == 0) {
			rm(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("mkdir", cmd) == 0) {
			mymkdir(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("rmdir", cmd) == 0) {
			myrmdir(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("ls", cmd) == 0) {
			ls(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("cat", cmd) == 0) {
			cat(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("cd", cmd) == 0) {
			cd(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("pwd", cmd) == 0) {
			pwd();
		}
		else if (strcmp("info", cmd) == 0) {
			info(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("incp", cmd) == 0) {
			incp(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("outcp", cmd) == 0) {
			outcp(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("load", cmd) == 0) {
			load(buffer + strlen(cmd) + 1);
		}
		else if (strcmp("format", cmd) == 0) {
			 format(DEFAULT_FS_SIZE);
		}
		else if (strcmp("defrag", cmd) == 0) {
			defrag(buffer + strlen(cmd) + 1);
		}
		else if (buffer[0] == 'q') {
			exit = 1;
		}
		else {
			printf("Unknown command.\n");
		}
		
	} while (!exit);
}

// Perform all needed operations before exiting
void shutdown() {
	if (sb) free(sb);
	if (bitmap) free(bitmap);
	if (inodes) free(inodes);
	fclose(fs);
}

// Entry point of the program
int main(int argc, char *argv[]) {
	
	// Test if argument exists
	if (argc < 2) {
		printf("No argument! Enter the filesystem name.\n");
		return EXIT_FAILURE;
	}
	
	// Test if argument is not too long
	if (strlen(argv[1]) > FS_NAME_MAX_L) {
		printf("Filesystem name is too long. [max. 11 chars]\n");
		return EXIT_FAILURE;
	}
	
	// Test if filesystem already exists
	if (access(argv[1], F_OK) == -1) {
		// Create new filesystem
		fs = fopen(argv[1], "w+");
		printf("Creating a new filesystem.\n");
		format(DEFAULT_FS_SIZE);
	}
	else {
		fs = fopen(argv[1], "r+");
	}
	
	run();
	shutdown();
	
	return EXIT_SUCCESS;
}
