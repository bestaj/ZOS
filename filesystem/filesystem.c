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
#include <limits.h>

#define NAME_MAX_LENGTH 11			// Maximum length of name (with extension)
#define BUFF_SIZE 256				// Buffer size for input commands
#define CLUSTER_SIZE 1024
#define BYTE 1
#define ERROR -1
#define NO_ERROR 0

#define FNF "FILE NOT FOUND\n"
#define PNF "PATH NOT FOUND\n"
#define EXIST "EXIST\n"
#define NE "NOT EMPTY\n"
#define OK "OK\n"
#define CCF "CANNOT CREATE FILE\n"
#define NES "FILESYSTEM HAS NOT ENOUGH SPACE\n"


struct superblock {
 //   char signature[9];              //login autora FS
 //   char volume_descriptor[251];    //popis vygenerovaného FS
 
    int32_t disk_size;              // Filesystem size
    int32_t cluster_size;           // Cluster size
    int32_t cluster_count;          // Count of clusters
    int32_t inode_count;			// Count of i-nodes
    int32_t bitmap_cluster_count;	// Count of clusters for bitmap
    int32_t inode_cluster_count;	// Count of clusters for i-nodes
    int32_t data_cluster_count;		// Count of clusters for data
    int32_t bitmap_start_address;   // Start address of the bitmap of the data blocks
    int32_t inode_start_address;    // Start address of the i-nodes
    int32_t data_start_address;     // Start address of data blocks  
};


typedef struct theinode {
    int32_t nodeid;                 // i-node ID, if ID = ID_ITEM_FREE, then i-node is free
    int8_t isDirectory;             // 0 = file, 1 = directory
    int8_t references;              // Count of references to i-node
    int32_t file_size;              // Size of the file/directory in bytes
    int32_t direct1;                // 1. direct reference to data blocks
    int32_t direct2;                // 2. direct reference to data blocks
    int32_t direct3;                // 3. direct reference to data blocks
    int32_t direct4;                // 4. direct reference to data blocks
    int32_t direct5;                // 5. direct reference to data blocks
    int32_t indirect1;              // 1. indirect reference 
	int32_t indirect2;              // 2. indirect reference 
} inode;


typedef struct thedirectory_item{
    int32_t inode;               	// i-node ID (index to array)
    char item_name[12];             // File name 8+3 + \0
	struct thedirectory_item *next;	// Reference to another directory item in the current directory 
} directory_item;


typedef struct thedirectory {
	struct thedirectory *parent;	// Reference to the parent directory
	directory_item *current;		// Current directory item
	directory_item *subdir;			// Reference to the first subdirectory in the list of all subdirectories in the current directory
	directory_item *file;			// Reference to the first file in the list of all files in the current directory
} directory;	

void cp(char *files);
void mv(char *files);
void rm(char *file);
void mymkdir(char *path);
void myrmdir(char *path);
void ls(char *path);
void cat(char *file);
void cd(char *path);
void pwd();
void info(char *file);
void incp(char *files);
void outcp(char *files);
FILE *load(char *file);
void format(long bytes);
void defrag();

int run();
void shutdown();
int32_t get_size(char *size);
int32_t find_free_inode();
int32_t *find_free_data_blocks(int count);
int parse_path(char *path, char **name, directory **dir);
int create_directory(directory *parent, char *name);
directory_item *create_directory_item(int32_t inode_id, char *name);
directory *find_directory(char *path);
void free_directories(directory *root);
void clear_inode(int id);
void update_sizes(directory *dir, int32_t size);
void print_info(directory_item *item);
void print_file(int32_t inode_id);
void print_format_msg();

void update_bitmap(int id, int action);
void update_inode(int id);
void load_fs();
void load_directory(directory *dir, int id);
int update_directory(int32_t id, directory_item *item, int action);

const int32_t INODE_SIZE = 38;			// Size of the i-node in bytes
const int32_t FREE = -1;					// item is free
const char *DELIM = " \n"; 

FILE *fs;								// File with filesystem
struct superblock *sb;					// Superblock
int8_t *bitmap = NULL;					// Bitmap of data blocks, 0 = free	1 = full
inode *inodes = NULL;					// Array of i-nodes, i-node ID = index to array
directory **directories = NULL;			// Array of pointers to directories, i-node ID = index to array
directory *working_directory;			// Current directory
int fs_formatted;						// If filesystem is formatted, 0 = false, 1 = true
char block_buffer[CLUSTER_SIZE];		// Buffer for one cluster 
int file_input = 0;




/* 	***************************************************
	Entry point of the program	
	
	param argv[1] ... name of the filesystem
*/
int main(int argc, char *argv[]) {
	
	// Test if argument exists
	if (argc < 2) {
		printf("No argument! Enter the filesystem name.\n");
		return EXIT_FAILURE;
	}
	printf("Filesystem is running...\n");
	
	// Test if filesystem already exists
	if (access(argv[1], F_OK) == -1) {
		// Create new filesystem
		fs_formatted = 0;
		printf("The filesystem has to be formatted first.\nUsage: format [size]\n");
		fs = fopen(argv[1], "wb+");
	}
	else {
		fs_formatted = 1;
		fs = fopen(argv[1], "rb+");
		load_fs();
	}
	
	run();
	shutdown();
	
	return EXIT_SUCCESS;
}


/*	Process user commands */
int run() {
	short exit = 0;
	char buffer[BUFF_SIZE] = {0};
	char *cmd, *args;
	FILE *f;
	int32_t size;	// Size of the filesystem
	
	do {
		// Load user command into buffer
		memset(buffer, 0, BUFF_SIZE);

		if (file_input) {	// Commands from the file
			fgets(buffer, BUFF_SIZE, f);
			if (!feof(f)) {
				printf("%s", buffer);
			}
			else {
				file_input = 0;
				fclose(f);
				continue;
			}
		}
		else {				// Commands from the console
			fgets(buffer, BUFF_SIZE, stdin);
		}
		
		if (buffer[0] == '\n')
			continue;
		

		cmd = strtok(buffer, DELIM);
		args = strtok(NULL, "\n");
		
		if (strcmp("cp", cmd) == 0) {
			cp(args);
		}
		else if (strcmp("mv", cmd) == 0) {
			mv(args);
		}
		else if (strcmp("rm", cmd) == 0) {
			rm(args);
		}
		else if (strcmp("mkdir", cmd) == 0) {
			mymkdir(args);
		}
		else if (strcmp("rmdir", cmd) == 0) {
			myrmdir(args);
		}
		else if (strcmp("ls", cmd) == 0) {
			ls(args);
		}
		else if (strcmp("cat", cmd) == 0) {
			cat(args);
		}
		else if (strcmp("cd", cmd) == 0) {
			cd(args);
		}
		else if (strcmp("pwd", cmd) == 0) {
			pwd();
		}
		else if (strcmp("info", cmd) == 0) {
			info(args);
		}
		else if (strcmp("incp", cmd) == 0) {
			incp(args);
		}
		else if (strcmp("outcp", cmd) == 0) {
			outcp(args);
		}
		else if (strcmp("load", cmd) == 0) {
			f = load(args);
		}
		else if (strcmp("format", cmd) == 0) {
			size = get_size(buffer + strlen(cmd) + 1);
			if (size == -1)		// Problem with the size of the filesystem
				continue;
			format(size);
		}
		else if (strcmp("defrag", cmd) == 0) {
			defrag();
		}
		else if (buffer[0] == 'q') {
			exit = 1;
		}
		else {
			printf("UNKNOWN COMMAND\n");
		}
	} while (!exit);
}

/* Perform all needed operations before exit */
void shutdown() {
	if (sb) free(sb);
	if (bitmap) free(bitmap);
	if (inodes) free(inodes);
	free_directories(directories[0]);
	free(directories);
	fclose(fs);
}

/*	******************************** Filesystem commands *********************************	*/

void cp(char *files) {
	int i, block_count, rest, tmp, tmp2, last_block;
	int32_t *source_blocks, *dest_blocks, inode_id;
	char *source, *dest, *name;
	directory *source_dir, *dest_dir;
	directory_item *item, **pitem;
	inode source_node;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	source = strtok(files, " ");
	dest = strtok(NULL, "\n");
	
	if (parse_path(source, &name, &source_dir)) {
		printf(FNF);
		return;
	}
	
	item = source_dir->file;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			source_node = inodes[item->inode];
			break;
		}
		item = item->next;
	}
	
	if (!item) {
		printf(FNF);
		return;
	}
	
	dest_dir = find_directory(dest);
	if (!dest_dir) {
		printf(PNF);
		return;
	}
	
	
	block_count = source_node.file_size / CLUSTER_SIZE;
	rest = source_node.file_size % CLUSTER_SIZE;
	if (rest != 0)
		block_count++;
		
	source_blocks = (int32_t *)malloc(block_count);
	
	source_blocks[0] = source_node.direct1;
	if (block_count > 1) {
		source_blocks[1] = source_node.direct2;
		if (block_count > 2) {
			source_blocks[2] = source_node.direct3;
			if (block_count > 3) {
				source_blocks[3] = source_node.direct4;
				if (block_count > 4) {
					source_blocks[4] = source_node.direct5;
					if (block_count > 5) {
						if (block_count > 261) {
							fseek(fs, sb->data_start_address + source_node.indirect1 * CLUSTER_SIZE, SEEK_SET);
							fread(&source_blocks[5], sizeof(int32_t), 256, fs);
							
							tmp = block_count - 261;
							fseek(fs, sb->data_start_address + source_node.indirect2 * CLUSTER_SIZE, SEEK_SET);
							fread(&source_blocks[261], sizeof(int32_t), tmp, fs);
						}
						else {
							tmp = block_count - 5;
							fseek(fs, sb->data_start_address + source_node.indirect1 * CLUSTER_SIZE, SEEK_SET);
							fread(&source_blocks[5], sizeof(int32_t), tmp, fs);
						}
					}
				}
			}
		}
	}
	
	if (block_count < 5)	// Use only direct references
		tmp = block_count;
	else if ((block_count > 5) && (block_count < 262))	// Use first indirect reference (+1 data block)
		tmp = block_count + 1;
	else 
		tmp = block_count + 2;			// Use both indirect references (+2 data block)
	
	dest_blocks = find_free_data_blocks(tmp);
	if (!dest_blocks) {
		printf(NES);
		return;
	}
	
	inode_id = find_free_inode();
	if (inode_id == ERROR) {
		printf(NES);
		return;
	}
	
	pitem = &(dest_dir->file);
	while (*pitem != NULL) {
		pitem = &((*pitem)->next);
	}
	*pitem = create_directory_item(inode_id, name);
		
	inodes[inode_id].nodeid = inode_id;
	inodes[inode_id].isDirectory = 0;
	inodes[inode_id].references = 1;
	inodes[inode_id].file_size = source_node.file_size;
	inodes[inode_id].direct1 = dest_blocks[0];
	last_block = 0;
	if (block_count > 1) {
		inodes[inode_id].direct2 = dest_blocks[1];
		last_block = 1;
		if (block_count > 2) {
			inodes[inode_id].direct3 = dest_blocks[2];
			last_block = 2;
			if (block_count > 3) {
				inodes[inode_id].direct4 = dest_blocks[3];
				last_block = 3;
				if (block_count > 4) {
					inodes[inode_id].direct5 = dest_blocks[4];
					last_block = 4;
					if (block_count > 5) {
						inodes[inode_id].indirect1 = dest_blocks[tmp - 1];
						last_block = tmp - 2;
						
						if (block_count > 261) {
							inodes[inode_id].indirect2 = dest_blocks[tmp - 2];
							last_block = tmp - 3;
							fseek(fs, sb->data_start_address + inodes[inode_id].indirect1 * CLUSTER_SIZE, SEEK_SET);
							fwrite(&dest_blocks[5], sizeof(int32_t), 256, fs);
							
							tmp2 = block_count - 261;
							fseek(fs, sb->data_start_address + inodes[inode_id].indirect2 * CLUSTER_SIZE, SEEK_SET);
							fwrite(&dest_blocks[261], sizeof(int32_t), tmp2, fs);
						}
						else  {
							tmp2 = block_count - 5;
							fseek(fs, sb->data_start_address + inodes[inode_id].indirect1 * CLUSTER_SIZE, SEEK_SET);
							fwrite(&dest_blocks[5], sizeof(int32_t), tmp2, fs);
						}
					}
				}
			}
		}
	}
	
	for (i = 0; i < tmp; i++) {
		bitmap[dest_blocks[i]] = 1;
	}
	
	update_bitmap(inode_id, 1);
	update_inode(inode_id);
	update_directory(dest_dir->current->inode, *pitem, 1);
	update_sizes(dest_dir, source_node.file_size);
	
	for (i = 0; i < block_count - 1; i++) {
		fseek(fs, sb->data_start_address + source_blocks[i] * CLUSTER_SIZE, SEEK_SET);
		fflush(fs);
		fread(block_buffer, sizeof(block_buffer), 1, fs);
		fseek(fs, sb->data_start_address + dest_blocks[i] * CLUSTER_SIZE, SEEK_SET);
		fflush(fs);
		fwrite(block_buffer, sizeof(block_buffer), 1, fs);
	}
	
	memset(block_buffer, 0, CLUSTER_SIZE);
	if (rest != 0)
		tmp = rest;
	else 
		tmp = CLUSTER_SIZE;
	
	fseek(fs, sb->data_start_address + source_blocks[block_count - 1] * CLUSTER_SIZE, SEEK_SET);
	fflush(fs);
	fread(block_buffer, tmp, 1, fs);
	fseek(fs, sb->data_start_address + dest_blocks[last_block] * CLUSTER_SIZE, SEEK_SET);
	fflush(fs);
	fwrite(block_buffer, tmp, 1, fs);
	fflush(fs);
	
	free(source_blocks);
	free(dest_blocks);
	
	printf(OK);
	
}


void mv(char *files) {
	char *source, *dest, *name;
	directory *source_dir, *dest_dir;
	directory_item *item, **pitem2, **temp;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	source = strtok(files, " ");
	dest = strtok(NULL, "\n");
	
	if (parse_path(source, &name, &source_dir)) {
		printf(FNF);
		return;
	}
	
	temp = &(source_dir->file); // Previous item 
	item = source_dir->file;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			(*temp) = item->next;
			update_directory(source_dir->current->inode, item, 0);
			update_sizes(source_dir, -(inodes[item->inode].file_size));
			break;
		}
		temp = &(item->next);
		item = item->next;
	}
	
	if (!item) {
		printf(FNF);
		return;
	}
	
	dest_dir = find_directory(dest);
	if (!dest_dir) {
		printf(PNF);
		return;
	}
	
	pitem2 = &(dest_dir->file);
	while (*pitem2 != NULL) {
		pitem2 = &((*pitem2)->next);
	}
	*pitem2 = item;
	update_directory(dest_dir->current->inode, item, 1);
	update_sizes(dest_dir, inodes[item->inode].file_size);
	
	printf(OK);
}


void rm(char *path) {
	int i, block_count, rest, tmp;
	int32_t *blocks;
	inode node;
	directory *dir;
	char *name;
	directory_item *item, **temp;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (parse_path(path, &name, &dir)) {
		printf(FNF);
		return;
	}

	temp = &(dir->file);
	item = dir->file;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			node = inodes[item->inode];
			(*temp) = item->next;
			break;
		}
		(*temp) = item->next;
		item = item->next;
	}
	
	if (!item) {
		printf(FNF);
		return;
	}
	
	update_directory(dir->current->inode, item, 0);
	free(item);
	
	block_count = node.file_size / CLUSTER_SIZE;
	rest = node.file_size % CLUSTER_SIZE;
	if (rest != 0)
		block_count++;
		
	blocks = (int32_t *)malloc(block_count);
	blocks[0] = node.direct1;
	if (block_count > 1) {
		blocks[1] = node.direct2;
		if (block_count > 2) {
			blocks[2] = node.direct3;
			if (block_count > 3) {
				blocks[3] = node.direct4;
				if (block_count > 4) {
					blocks[4] = node.direct5;
					if (block_count > 5) {
						if (block_count > 261) {
							fseek(fs, sb->data_start_address + node.indirect1 * CLUSTER_SIZE, SEEK_SET);
							fread(&blocks[5], sizeof(int32_t), 256, fs);
							
							tmp = block_count - 261;
							fseek(fs, sb->data_start_address + node.indirect2 * CLUSTER_SIZE, SEEK_SET);
							fread(&blocks[261], sizeof(int32_t), tmp, fs);
						}
						else {
							tmp = block_count - 5;
							fseek(fs, sb->data_start_address + node.indirect1 * CLUSTER_SIZE, SEEK_SET);
							fread(&blocks[5], sizeof(int32_t), tmp, fs);
						}
					}
				}
			}
		}
	}
	
	for (i = 0; i < block_count; i++) {
		bitmap[blocks[i]] = 0;
	}
	update_bitmap(node.nodeid, 0);
	
	memset(block_buffer, 0, CLUSTER_SIZE);
	for (i = 0; i < block_count - 1; i++) {
		fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
		fwrite(block_buffer, sizeof(block_buffer), 1, fs);
	}
	
	if (rest != 0)
		tmp = rest;
	else 
		tmp = CLUSTER_SIZE;
	
	fseek(fs, sb->data_start_address + blocks[block_count - 1] * CLUSTER_SIZE, SEEK_SET);
	fwrite(block_buffer, tmp, 1, fs);
	fflush(fs);
	free(blocks);
	
	update_sizes(dir, -(node.file_size));

	clear_inode(node.nodeid);
	update_inode(node.nodeid);
	
	printf(OK);
}


/*	***************************************************
	Create a new directory
	
	param path ... path of the new directory
*/
void mymkdir(char *path) {
	directory *dir;			// Parent of adding directory
	directory_item *item;
	char *name;				// Name of the creating directory
	
	if (!fs_formatted) {	// Filesystem was not formatted yet
		print_format_msg();
		return;	
	}
	
	if (parse_path(path, &name, &dir)) {
		printf(PNF);
		return;
	}

	// If the file with the same name already exist 
	item = dir->file;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			printf(EXIST);
			return;
		}
		item = item->next;
	}
	
	// If the directory with the same name already exist
	item = dir->subdir;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {	
			printf(EXIST);
			return;
		}
		item = item->next;
	}
	
	if (create_directory(dir, name)) {
		printf(NES);
		return;
	}
	printf(OK);
}


/* 	***************************************************
	Remove the empty directory

	param path ... path to the removing directory
*/
void myrmdir(char *path) {
	directory *dir;
	directory_item *item, **temp;
	char *name;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (parse_path(path, &name, &dir)) {
		printf(PNF);
		return;
	}

	temp = &(dir->subdir); // Previous item 
	item = dir->subdir;	   // Current item
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			if ((directories[item->inode]->file != NULL) || (directories[item->inode]->subdir != NULL)) {		// If directory is not empty
				printf(NE);		
				return;			
			}

			(*temp) = item->next;
			
			if (working_directory == directories[item->inode]) {	// If removing working directory
				working_directory = directories[item->inode]->parent;
			}
			
			bitmap[inodes[item->inode].direct1] = 0;
			update_bitmap(item->inode, 0);
			
			clear_inode(item->inode);		// Set i-node as free
			update_inode(item->inode);
			
			update_directory(dir->current->inode, item, 0);
		
			free(directories[item->inode]->current);	// Clear directory
			free(directories[item->inode]);
			free(item);
			break;
		}
		temp = &(item->next);
		item = item->next;
	}
	if (!item) {	// If file was found
		printf(FNF);
		return;
	}

	printf(OK);
}


/*	***************************************************
	Print all items in the directory

	param path ... path of the directory
*/
void ls(char *path) {
	int i;
	directory *dir;
	directory_item *item;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (!path || path == "") {
		printf(PNF);
		return;
	}
	
	dir = find_directory(path);
	if (!dir) {
		printf(PNF);
		return;
	}
	
	item = dir->subdir;
	while (item != NULL) {	// Print all subdirectories
		printf("+%s\n", item->item_name);
		item = item->next;
	}

	item = dir->file;	
	while (item != NULL) {	// Print all files
		printf("-%s\n", item->item_name);
		item = item->next;
	}
}


void cat(char *path) {
	directory *dir;
	directory_item *item;
	char *name;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (parse_path(path, &name, &dir)) {
		printf(FNF);
		return;
	}
	
	item = dir->file;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			print_file(item->inode);
			printf("\n");
			return;
		}
		item = item->next;
	}
	printf(FNF);
}


/*	***************************************************
	Change the working directory according to the path

	param path ... path to the new working directory
*/
void cd(char *path) {
	directory *dir;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	dir = find_directory(path);
	if (!dir) {		
		printf(PNF);	// Path not found
		return;
	}
	
	working_directory = dir;
	printf(OK);
}


/*	***************************************************
	Print the path of the working directory
*/
void pwd() {
	char *names[32];	// Directory names on the path to the working directory
	int count = 0;
	int i;
	directory *temp = working_directory;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	while (temp != directories[0]) {	// Until get to root directory
		names[count] = temp->current->item_name;
		count++;
		temp = temp->parent;
	}
	
	printf("/");
	for (i = count - 1; i >= 0; i--) {
		printf("%s", names[i]);
		if (i != 0)
			printf("/");
	}
	printf("\n");
}


void info(char *path) {
	directory *dir;
	char *name;
	directory_item *item;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (parse_path(path, &name, &dir)) {
		printf(FNF);
		return;
	}
	
	// Finding item between files 
	item = dir->file;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			print_info(item);	
			return;
		}
		item = item->next;
	}
	
	// Finding item between subdirectories
	item = dir->subdir;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {	
			print_info(item);
			return;
		}
		item = item->next;
	}
}


void incp(char *files) {
	int32_t file_size, *blocks, inode_id;
	int block_count, rest, tmp, tmp2, i, last_block;
	char *source, *dest, *name;
	directory *dir; 
	directory_item **pitem;
	FILE *f;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	source = strtok(files, " ");
	dest = strtok(NULL, "\n");
	
	if ((name = strrchr(source, '/')) == NULL) {
		name = source;
	}
	else {
		name++;
	}
	
	if ((f = fopen(source, "rb")) == NULL) {
		printf(FNF);		// Source was not found
		return;
	}
	
	dir = find_directory(dest);
	if (!dir) {
		printf(PNF);		// Destination was not found
		return;
	}
	
	fseek(f, 0, SEEK_END);
	file_size = ftell(f);
	rewind(f);
	
	
	block_count = file_size / CLUSTER_SIZE;
	rest = file_size % CLUSTER_SIZE;
	
	if (rest != 0)
		block_count++;
	
	if (block_count < 5)	// Use only direct references
		tmp = block_count;
	else if ((block_count > 5) && (block_count < 262))	// Use first indirect reference (+1 data block)
		tmp = block_count + 1;
	else 
		tmp = block_count + 2;			// Use both indirect references (+2 data block)
	
	blocks = find_free_data_blocks(tmp);
	if (!blocks) {
		printf(NES);
		return;
	}
	
	inode_id = find_free_inode();
	if (inode_id == ERROR) {
		printf(NES);
		return;
	}
	
	pitem = &(dir->file);
	while (*pitem != NULL) {
		pitem = &((*pitem)->next);
	}
	*pitem = create_directory_item(inode_id, name);

	inodes[inode_id].nodeid = inode_id;
	inodes[inode_id].isDirectory = 0;
	inodes[inode_id].references = 1;
	inodes[inode_id].file_size = file_size;
	inodes[inode_id].direct1 = blocks[0];
	last_block = 0;
	if (block_count > 1) {
		inodes[inode_id].direct2 = blocks[1];
		last_block = 1;
		if (block_count > 2) {
			inodes[inode_id].direct3 = blocks[2];
			last_block = 2;
			if (block_count > 3) {
				inodes[inode_id].direct4 = blocks[3];
				last_block = 3;
				if (block_count > 4) {
					inodes[inode_id].direct5 = blocks[4];
					last_block = 4;
					if (block_count > 5) {
						inodes[inode_id].indirect1 = blocks[tmp - 1];
						last_block = tmp - 2;
						
						if (block_count > 261) {
							inodes[inode_id].indirect2 = blocks[tmp - 2];
							last_block = tmp - 3;
							fseek(fs, sb->data_start_address + inodes[inode_id].indirect1 * CLUSTER_SIZE, SEEK_SET);
							fwrite(&blocks[5], sizeof(int32_t), 256, fs);
							
							tmp2 = block_count - 261;
							fseek(fs, sb->data_start_address + inodes[inode_id].indirect2 * CLUSTER_SIZE, SEEK_SET);
							fwrite(&blocks[261], sizeof(int32_t), tmp2, fs);
						}
						else  {
							tmp2 = block_count - 5;
							fseek(fs, sb->data_start_address + inodes[inode_id].indirect1 * CLUSTER_SIZE, SEEK_SET);
							fwrite(&blocks[5], sizeof(int32_t), tmp2, fs);
						}
					}
				}
			}
		}
	}
	
	for (i = 0; i < tmp; i++) {
		bitmap[blocks[i]] = 1;
	}
	
	update_bitmap(inode_id, 1);
	update_inode(inode_id);
	update_directory(dir->current->inode, *pitem, 1);
	update_sizes(dir, file_size);
	
	for (i = 0; i < block_count - 1; i++) {
		fread(block_buffer, sizeof(block_buffer), 1, f);
		fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
		fwrite(block_buffer, sizeof(block_buffer), 1, fs);
	}
	
	memset(block_buffer, 0, CLUSTER_SIZE);
	if (rest != 0)
		tmp = rest;
	else 
		tmp = CLUSTER_SIZE;
	
	fread(block_buffer, sizeof(char), tmp, f);
	fseek(fs, sb->data_start_address + blocks[last_block] * CLUSTER_SIZE, SEEK_SET);
	fwrite(block_buffer, sizeof(char), tmp, fs);
	fflush(fs);
	
	fclose(f);
	free(blocks);
	printf(OK);
}


void outcp(char *files) {
	int i, block_count, rest, found = 0, tmp;
	int32_t *blocks;
	inode node;
	char *source, *dest, *name;
	char whole_dest[BUFF_SIZE];
	directory *dir;
	directory_item *item;
	FILE *f;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	source = strtok(files, " ");
	dest = strtok(NULL, "\n");
	
	if (parse_path(source, &name, &dir)) {
		printf(FNF);
		return;
	}

	memset(whole_dest, 0, BUFF_SIZE);
	sprintf(whole_dest, "%s/%s", dest, name);
	
	
	if ((f = fopen(whole_dest, "wb")) == NULL) {
		printf(PNF);		// Source was not found
		return;
	}
	
	item = dir->file;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			node = inodes[item->inode];
			found = 1;
			break;
		}
		item = item->next;
	}
	
	if (!found) {
		printf(FNF);
		return;
	}
	
	block_count = node.file_size / CLUSTER_SIZE;
	rest = node.file_size % CLUSTER_SIZE;
	if (rest != 0)
		block_count++;
		
	blocks = (int32_t *)malloc(block_count);
	blocks[0] = node.direct1;
	if (block_count > 1) {
		blocks[1] = node.direct2;
		if (block_count > 2) {
			blocks[2] = node.direct3;
			if (block_count > 3) {
				blocks[3] = node.direct4;
				if (block_count > 4) {
					blocks[4] = node.direct5;
					if (block_count > 5) {
						if (block_count > 261) {
							fseek(fs, sb->data_start_address + node.indirect1 * CLUSTER_SIZE, SEEK_SET);
							fread(&blocks[5], sizeof(int32_t), 256, fs);
							
							tmp = block_count - 261;
							fseek(fs, sb->data_start_address + node.indirect2 * CLUSTER_SIZE, SEEK_SET);
							fread(&blocks[261], sizeof(int32_t), tmp, fs);
						}
						else {
							tmp = block_count - 5;
							fseek(fs, sb->data_start_address + node.indirect1 * CLUSTER_SIZE, SEEK_SET);
							fread(&blocks[5], sizeof(int32_t), tmp, fs);
						}
					}
				}
			}
		}
	}
	
	for (i = 0; i < block_count - 1; i++) {
		fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
		fread(block_buffer, sizeof(block_buffer), 1, fs);
		fwrite(block_buffer, sizeof(block_buffer), 1, f);
	}
	
	memset(block_buffer, 0, CLUSTER_SIZE);
	if (rest != 0)
		tmp = rest;
	else 
		tmp = CLUSTER_SIZE;
	
	fseek(fs, sb->data_start_address + blocks[block_count - 1] * CLUSTER_SIZE, SEEK_SET);
	fread(block_buffer, tmp, 1, fs);
	fwrite(block_buffer, tmp, 1, f);
	
	fflush(fs);
	fclose(f);
	free(blocks);
		
	printf(OK);
}


FILE *load(char *file) {
	FILE *f;
	
	if (!fs_formatted) {
		print_format_msg();
		return NULL;	
	}

	if ((f = fopen(file, "r")) == NULL) {
		printf(FNF);		// Source was not found
		return NULL;
	}
	file_input = 1;
	
	printf(OK);
	return f;
}


/* 	***************************************************
	Format existing filesystem or create a new one with specific size

	param bytes ... size of the filesystem in bytes
*/
void format(long bytes) {
	int i;
	directory *root;
	
	printf("Formating...\n");
	
	// Prepare superblock
	if (!fs_formatted) {		// If not exist -> create
		sb = (struct superblock *)malloc(sizeof(struct superblock));
		if (!sb) {
			printf(CCF);
			return;
		}
	}
	
	sb->cluster_size = CLUSTER_SIZE;											// Size of the cluster
	sb->cluster_count = bytes / CLUSTER_SIZE; 									// Count of all clusters
	sb->disk_size = sb->cluster_count * CLUSTER_SIZE; 							// Exact size of the filesystem in bytes
	sb->inode_cluster_count = sb->cluster_count / 20; 							// Count of blocks for i-nodes, 5% of all blocks
	sb->inode_count = (sb->inode_cluster_count * CLUSTER_SIZE) / INODE_SIZE;	// Count of i-nodes
	sb->bitmap_start_address = CLUSTER_SIZE; 									// Initial address of bitmap blocks
	sb->bitmap_cluster_count = ceil((sb->cluster_count - sb->inode_cluster_count - 1) / (float)CLUSTER_SIZE);	// Count of blocks for bitmap to cover all data blocks
	sb->data_cluster_count = sb->cluster_count - 1 - sb->bitmap_cluster_count - sb->inode_cluster_count;		// Count of data blocks
	sb->inode_start_address = sb->bitmap_start_address + CLUSTER_SIZE * sb->bitmap_cluster_count;				// Initial address of i-node blocks
	sb->data_start_address = sb->inode_start_address + CLUSTER_SIZE * sb->inode_cluster_count;					// Initial address of data blocks
	
	
	printf("Size: %d\nCount of clusters: %d\nCount of i-nodes: %d\nCount of bitmap blocks: %d\nCount of i-node blocks: %d\nCount of data blocks: %d\nAddress of bitmap: %d\nAddress of i-nodes: %d\nAddress of data: %d\n", 
	sb->disk_size, sb->cluster_count, sb->inode_count, sb->bitmap_cluster_count, sb->inode_cluster_count, sb->data_cluster_count, sb->bitmap_start_address, sb->inode_start_address, sb->data_start_address);
	
	if (fs_formatted) {		// If filesystem has already been formatted 
		free(bitmap);
		free_directories(directories[0]);
		free(directories);
		free(inodes);
	}
	
	// Prepare bitmap, i-nodes and pointers to directories
	bitmap = (int8_t *)malloc(sb->data_cluster_count);
	inodes = (inode *)malloc(sizeof(inode) * sb->inode_count);
	directories = (directory **)malloc(sizeof(directory *) * sb->inode_count);
	if (!bitmap || !inodes || !directories) {
		printf(CCF);
		return;
	}
	
	// Create root directory
	root = (directory *)malloc(sizeof(directory));
	if (!root) {
		printf(CCF);
		return;
	}
	root->current = create_directory_item(0,"/");
	root->parent = root;
	root->subdir = NULL;
	root->file = NULL;

	working_directory = root;	// Set root as working directory
	directories[0] = root;
	
	bitmap[0] = 1;

	// Clear bitmap (except root)
	for (i = 1; i < sb->data_cluster_count; i++) {
		bitmap[i] = 0;
	}

	// Set all i-nodes as free 
	for (i = 0; i < sb->inode_count; i++) {
		inodes[i].nodeid = FREE;
		inodes[i].isDirectory = 0;
		inodes[i].references = 0;
		inodes[i].file_size = 0;
		inodes[i].direct1 = FREE;
		inodes[i].direct2 = FREE;
		inodes[i].direct3 = FREE;
		inodes[i].direct4 = FREE;
		inodes[i].direct5 = FREE;
		inodes[i].indirect1 = FREE;
		inodes[i].indirect2 = FREE;
	}
	
	// Set root i-node
	inodes[0].nodeid = 0;
	inodes[0].isDirectory = 1;
	inodes[0].references = 1;
	inodes[0].direct1 = 0;
	
	// Fill the file by zeros
	memset(block_buffer, 0, CLUSTER_SIZE);
	for (i = 0; i < sb->cluster_count; i++) {
		fwrite(block_buffer, sizeof(block_buffer), 1, fs);		
	}
	
	// Store the superblock
	rewind(fs);
	fwrite(&(sb->disk_size), sizeof(int32_t), 1, fs);
	fwrite(&(sb->cluster_size), sizeof(int32_t), 1, fs);
	fwrite(&(sb->cluster_count), sizeof(int32_t), 1, fs);
	fwrite(&(sb->inode_count), sizeof(int32_t), 1, fs);
	fwrite(&(sb->bitmap_cluster_count), sizeof(int32_t), 1, fs);
	fwrite(&(sb->inode_cluster_count), sizeof(int32_t), 1, fs);
	fwrite(&(sb->data_cluster_count), sizeof(int32_t), 1, fs);
	fwrite(&(sb->bitmap_start_address), sizeof(int32_t), 1, fs);
	fwrite(&(sb->inode_start_address), sizeof(int32_t), 1, fs);
	fwrite(&(sb->data_start_address), sizeof(int32_t), 1, fs);
	
	// Store bitmap
	update_bitmap(0, 1);
	
	// Store i-nodes
	for (i = 0; i < sb->inode_count; i++) {
		update_inode(i);
	}
	
	fs_formatted = 1;
	printf(OK);
}


void defrag(char *cmd) {
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
}


/* 	**************************************************************************************	*/

/*	Validate entered size of the filesystem
	and convert it into bytes
	
	param size ... size of the filesystem as the string
	return ... size of the filesystem in bytes
*/
int32_t get_size(char *size) {
	char *units = NULL;		// Units
	long number;
	
	if (!size || size == "") {
		printf(CCF);
		return -1;
	}
	
	number = strtol(size, &units, 0);	// Convert to number
	
	if (number == 0 || errno != 0) {
		printf(CCF);
		return -1;
	}
	
	if (strncmp("KB", units, 2) == 0) {
		number = number * 1000;
	}
	else if (strncmp("MB", units, 2) == 0) {
		number = number * 1000000;
	}
	else if (strncmp("GB", units, 2) == 0) {
		number = number * 1000000000;
	}
	
	if (number < 20480) {	// If the size is enough 
		printf(CCF);
		return -1;
	}
	else if (number > INT_MAX) {	// If the size is too large
		printf(CCF);
		return -1;
	}
	
	return (int32_t)number;
}


/*	Find free i-node
	
	return ... i-node ID, if 0 = none i-node is free
*/
int32_t find_free_inode() {
	int i;
	
	for (i = 1; i < sb->inode_count; i++) {	// Finding a free i-node
		if (inodes[i].nodeid == FREE) {
			return i;
		}
	}
	return ERROR;
}


/* 	Find free data block in bitmap	*/
int32_t *find_free_data_blocks(int count) {
	int i, c = 0;
	int32_t *blocks = (int32_t *)malloc(sizeof(int32_t) * count);
		
	for (i = 1; i < sb->data_cluster_count; i++) {
		if (bitmap[i] == 0) {
			blocks[c] = i;
			c++;
			if (c == count)
				return blocks;
		}
	}
	free(blocks);
	return NULL;	// Nezapomenout uvolnit pamet pro blocks
}


int parse_path(char *path, char **name, directory **dir) {
	int length;				// Length of the path (without name of the new directory)
	char buff[BUFF_SIZE];
	
	if (!path || path == "") {
		return ERROR;
	}
	
	if ((*name = strrchr(path, '/')) == NULL) {	// Path contains only directory/file name
		*name = path;
		*dir = working_directory;
	}
	else  {
		// Separate the name of the directory/file from the rest of the path
		length = strlen(path) - strlen(*name);
		if (path[0] == '/') {					// If the path contains only root directory (and name of the creating directory)
			if (!strchr(path + 1, '/'))
				length = 1;
		}
		
		*name = *name + 1;
		memset(buff, '\0', BUFF_SIZE);
		strncpy(buff, path, length);
		
		*dir = find_directory(buff);		// Find the directory
		if (!(*dir)) {
			return ERROR;
		}
	}

	return NO_ERROR;
}



/*	Create a new directory
	
	param parent ... parent directory
	param name ... directory name
*/
int create_directory(directory *parent, char *name) {
	int32_t inode_id, *data_block;
	directory_item **temp;

	inode_id = find_free_inode();
	if (inode_id == ERROR) return ERROR;	// No free i-node
	
	data_block = find_free_data_blocks(1);
	if (data_block == NULL) return ERROR; 	// No free data block

	directory *newdir = (directory *)malloc(sizeof(directory));
	newdir->parent = parent;
	newdir->current = create_directory_item(inode_id, name);
	newdir->file = NULL;
	newdir->subdir = NULL;
	
	directories[inode_id] = newdir;
	bitmap[data_block[0]] = 1;
	
	inodes[inode_id].nodeid = inode_id;
	inodes[inode_id].isDirectory = 1;
	inodes[inode_id].references = 1;
	inodes[inode_id].file_size = 0;
	inodes[inode_id].direct1 = data_block[0];
	inodes[inode_id].direct2 = FREE;
	inodes[inode_id].direct3 = FREE;
	inodes[inode_id].direct4 = FREE;
	inodes[inode_id].direct5 = FREE;
	inodes[inode_id].indirect1 = FREE;
	inodes[inode_id].indirect2 = FREE;
	
	temp = &(parent->subdir);
	while (*temp != NULL) {
		temp = &((*temp)->next);
	}
	
	*temp = directories[inode_id]->current;

	if (update_directory(parent->current->inode, newdir->current, 1)) {
		return ERROR;	// No free data block for extending parent data blocks
	}
	update_inode(inode_id);
	update_bitmap(inode_id, 1);
	
	free(data_block);
	
	return NO_ERROR;
}


/* 	Create a new directory item 

	param inode_id ... i-node ID
	param name ... name of the file/directory
*/
directory_item *create_directory_item(int32_t inode_id, char *name) {
	char buff[12] = {'\0'};
	
	directory_item *dir_item = (directory_item *)malloc(sizeof(directory_item));

	strncpy(buff, name, strlen(name));
	dir_item->inode = inode_id;
	strncpy(dir_item->item_name, buff, 12);
	dir_item->next = NULL;
	
	return dir_item;
}


/*	Find an entered directory according to the path
	
	return ... directory or NULL if wasn't found 
*/
directory *find_directory(char *path) {
	int found;	// If directory was found
	char *part;	// Part of the path
	char *delim = "/";
	directory *dir;
	directory_item *item;
	
	if (path[0] == '/') {	// Absolute path
		dir = directories[0];
	}
	else {					// Relative path
		dir = working_directory;
	}

	part = strtok(path, delim);
	while (part != NULL) {
		if (strcmp(part, ".") == 0) {	// The same directory
			part = strtok(NULL, delim);
			continue;
		}
		else if (strcmp(part, "..") == 0) {	// Go to the parent directory
			dir = dir->parent;
			part = strtok(NULL, delim);
			continue;
		}
		else {
			found = 0;
			item = dir->subdir;
			while (item != NULL) {
				if (strcmp(part, directories[item->inode]->current->item_name) == 0) {
					dir = directories[item->inode];
					part = strtok(NULL, delim);
					found = 1;
					break;
				}
				item = item->next;
			}
			
			if (found == 0) {	// No such directory wasn't found
				return NULL;
			}
		}
	}
	return dir;
}


/*
	Free allocated memory for directories
*/
void free_directories(directory *root) {
	directory_item *f, *d, *t;
	
	if (root) return;
	
	d = root->subdir;
	while (d != NULL) {
		free_directories(directories[d->inode]);
		d = d->next;
	}

	f = root->file;
	while (f != NULL) {
		t = f->next;
		free(f);
		f = NULL;
		f = t;
	}
	
	free(d);
	d = NULL;
	free(root->current);
	free(root);
	root = NULL;
}

/* Initialize i-node with specific ID to free */ 
void clear_inode(int id) {
	inodes[id].nodeid = FREE;
	inodes[id].isDirectory = 0;
	inodes[id].references = 0;
	inodes[id].file_size = 0;
	inodes[id].direct1 = FREE;
	inodes[id].direct2 = FREE;
	inodes[id].direct3 = FREE;
	inodes[id].direct4 = FREE;
	inodes[id].direct5 = FREE;
	inodes[id].indirect1 = FREE;
	inodes[id].indirect2 = FREE;
}


void update_sizes(directory *dir, int32_t size) {
	directory *d = dir;
	while (d != directories[0]) {
		inodes[d->current->inode].file_size += size;
		update_inode(d->current->inode);
		d = d->parent;
	}
	
	inodes[d->current->inode].file_size += size;
	update_inode(d->current->inode);	
}


void print_info(directory_item *item) {
	int i, max_in_block = 256; // Maximum nambers in one data block
	int32_t number; // Data block number
	inode node = inodes[item->inode];
	
	printf("%s - %dB - i-node %d - %d", item->item_name, node.file_size, node.nodeid, node.direct1);
	
	if (node.direct2 != FREE) {
		printf(" %d", node.direct2);
		if (node.direct3 != FREE) {
			printf(" %d", node.direct3);
			if (node.direct4 != FREE) {
				printf(" %d", node.direct4);
				if (node.direct5 != FREE) {
					printf(" %d", node.direct5);
					if (node.indirect1 != FREE) {
						fseek(fs, sb->data_start_address + node.indirect1 * CLUSTER_SIZE, SEEK_SET);
						for (i = 0; i < max_in_block; i++) {
							fread(&number, sizeof(int32_t), 1, fs);
							if (number == 0) 
								break;
							printf(" %d", number);
						}
						if (node.indirect2 != FREE) {
							fseek(fs, sb->data_start_address + node.indirect2 * CLUSTER_SIZE, SEEK_SET);
							for (i = 0; i < max_in_block; i++) {
								fread(&number, sizeof(int32_t), 1, fs);
								if (number == 0) 
									break;
								printf(" %d", number);
							}
						}
					}
				}
			}
		}
	}
	printf("\n");
}


void print_file(int32_t inode_id) {
	int block_count, rest, tmp_count, i, tmp;
	int32_t *blocks;
	 
	block_count = inodes[inode_id].file_size / CLUSTER_SIZE;
	rest = inodes[inode_id].file_size % CLUSTER_SIZE;
	if (rest != 0)
		block_count++;

	blocks = (int32_t *)malloc(sizeof(int32_t) * block_count);
		 
	blocks[0] = inodes[inode_id].direct1;
	if (inodes[inode_id].direct2 != FREE) {
		blocks[1] = inodes[inode_id].direct2;
		if (inodes[inode_id].direct3 != FREE) {
			blocks[2] = inodes[inode_id].direct3;
			if (inodes[inode_id].direct4 != FREE) {
				blocks[3] = inodes[inode_id].direct4;
				if (inodes[inode_id].direct5 != FREE) {
					blocks[4] = inodes[inode_id].direct5;
					if (inodes[inode_id].indirect1 != FREE) {
						if (block_count > 261) {
							tmp_count = 256;
						}
						else {
							tmp_count = block_count - 5;
						}	
						
						fseek(fs, sb->data_start_address + inodes[inode_id].indirect1 * CLUSTER_SIZE, SEEK_SET);
						fread(&blocks[5], sizeof(int32_t), tmp, fs);
						
						if (inodes[inode_id].indirect2 != FREE) {
							tmp_count = block_count - 261;
							fseek(fs, sb->data_start_address + inodes[inode_id].indirect2 * CLUSTER_SIZE, SEEK_SET);
							fread(&blocks[261], sizeof(int32_t), tmp, fs);
						}
					}
				}
			}
		}
	}
		
	for (i = 0; i < block_count - 1; i++) {
		fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
		fread(block_buffer, sizeof(char), CLUSTER_SIZE, fs);
		printf("%s", block_buffer);	
	}
	memset(block_buffer, 0, CLUSTER_SIZE);
	if (rest != 0)
		tmp = rest;
	else 
		tmp = CLUSTER_SIZE;
		
	fseek(fs, sb->data_start_address + blocks[block_count - 1] * CLUSTER_SIZE, SEEK_SET);
	fread(block_buffer, sizeof(char), tmp, fs);
	printf("%s", block_buffer);	
	
	fflush(fs);
	
	free(blocks);
}


/* Printf the message of unformatted filesystem. */
void print_format_msg() {
	printf("The filesystem has to be formatted first.\nUsage: format [size]\n");
}



void load_fs() {
	directory *root;
	int i;
	
	// Load superblock
	sb = (struct superblock *)malloc(sizeof(struct superblock));
	if (!sb) {
		printf("Filesystem loading failed.\n");
		return;
	}
	
	fread(&(sb->disk_size), sizeof(int32_t), 1, fs);
	fread(&(sb->cluster_size), sizeof(int32_t), 1, fs);
	fread(&(sb->cluster_count), sizeof(int32_t), 1, fs);
	fread(&(sb->inode_count), sizeof(int32_t), 1, fs);
	fread(&(sb->bitmap_cluster_count), sizeof(int32_t), 1, fs);
	fread(&(sb->inode_cluster_count), sizeof(int32_t), 1, fs);
	fread(&(sb->data_cluster_count), sizeof(int32_t), 1, fs);
	fread(&(sb->bitmap_start_address), sizeof(int32_t), 1, fs);
	fread(&(sb->inode_start_address), sizeof(int32_t), 1, fs);
	fread(&(sb->data_start_address), sizeof(int32_t), 1, fs);
	
	printf("Size: %d\nCount of clusters: %d\nCount of i-nodes: %d\nCount of bitmap blocks: %d\nCount of i-node blocks: %d\nCount of data blocks: %d\nAddress of bitmap: %d\nAddress of i-nodes: %d\nAddress of data: %d\n", 
	sb->disk_size, sb->cluster_count, sb->inode_count, sb->bitmap_cluster_count, sb->inode_cluster_count, sb->data_cluster_count, sb->bitmap_start_address, sb->inode_start_address, sb->data_start_address);
	
	bitmap = (int8_t *)malloc(sb->data_cluster_count);
	inodes = (inode *)malloc(sizeof(inode) * sb->inode_count);
	directories = (directory **)malloc(sizeof(directory *) * sb->inode_count);
	if (!bitmap || !inodes || !directories) {
		printf(CCF);
		return;
	}
	
	// Load bitmap
	fseek(fs, sb->bitmap_start_address, SEEK_SET);
	fread(bitmap, sizeof(int8_t), sb->data_cluster_count, fs);
	
	// Load i-nodes
	fseek(fs, sb->inode_start_address, SEEK_SET);
	for (i = 0; i < sb->inode_count; i++) {
		fread(&(inodes[i].nodeid), sizeof(int32_t), 1, fs);
		fread(&(inodes[i].isDirectory), sizeof(int8_t), 1, fs);
		fread(&(inodes[i].references), sizeof(int8_t), 1, fs);
		fread(&(inodes[i].file_size), sizeof(int32_t), 1, fs);
		fread(&(inodes[i].direct1), sizeof(int32_t), 1, fs);
		fread(&(inodes[i].direct2), sizeof(int32_t), 1, fs);
		fread(&(inodes[i].direct3), sizeof(int32_t), 1, fs);
		fread(&(inodes[i].direct4), sizeof(int32_t), 1, fs);
		fread(&(inodes[i].direct5), sizeof(int32_t), 1, fs);
		fread(&(inodes[i].indirect1), sizeof(int32_t), 1, fs);
		fread(&(inodes[i].indirect2), sizeof(int32_t), 1, fs);
	}
	
	// Create root directory
	root = (directory *)malloc(sizeof(directory));
	if (!root) {
		printf(CCF);
		return;
	}
	root->current = create_directory_item(0,"/");
	root->parent = root;
	root->subdir = NULL;
	root->file = NULL;

	working_directory = root;	// Set root as working directory
	directories[0] = root;
	
	// Load directories
	load_directory(root, 0);
}


void load_directory(directory *dir, int id) {
	int i, j, counter = 0;
	int inode_count = 64;	// Maximum count of i-nodes in one data block
	int blocks[5];	// Numbers of data blocks
	directory_item **psubdir = &(dir->subdir);
	directory_item **pfile = &(dir->file);
	directory_item *item, *temp;
	directory *newdir;
	
	int32_t nodeid;		// Readed id from the file
	char name[12];		// Readed name from the file
	
	// Store numbers of data blocks to read
	if (inodes[id].direct1 != FREE) {
		blocks[0] = inodes[id].direct1;
		counter++;
		if (inodes[id].direct2 != FREE) {
			blocks[0] = inodes[id].direct2;
			counter++;
			if (inodes[id].direct3 != FREE) {
				blocks[0] = inodes[id].direct3;
				counter++;
				if (inodes[id].direct4 != FREE) {
					blocks[0] = inodes[id].direct4;
					counter++;
					if (inodes[id].direct5 != FREE) {
						blocks[0] = inodes[id].direct5;
						counter++;
						
						// + precist datove bloky z neprimych odkazu
					}
				}
			}
		}
	}
	
	for (i = 0; i < counter; i++) {		// Iteration over data blocks
		fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
		for (j = 0; j < inode_count; j++) {	// Iteration over items in data block
			fread(&nodeid, sizeof(int32_t), 1, fs);		// Read inode id, if id < 1 -> invalid item and skip to the next item
			if (nodeid > 0) {
				fread(name, sizeof(name), 1, fs);
				item = create_directory_item(nodeid, name);
				if (inodes[nodeid].isDirectory) {	// If item is directory or file
					*psubdir = item;
					psubdir = &(item->next);
				}
				else {
					*pfile = item;
					pfile = &(item->next);
				}
			}
			else {
				fseek(fs, sizeof(name), SEEK_CUR);	// Skip the place for the name of the file/directory
			} 
		}	
	}
	
	temp = dir->subdir;

	while (temp != NULL) {
		newdir = (directory *)malloc(sizeof(directory));
		newdir->parent = dir;
		newdir->current = temp;
		newdir->subdir = NULL;
		newdir->file = NULL;
		
		directories[temp->inode] = newdir;
		load_directory(newdir, temp->inode);
		
		temp = temp->next;
	}
}


/*	Update bitmap in the file

	param id ... i-node id
	param action ... 1 = use data blocks, 0 = free data blocks
*/
void update_bitmap(int id, int action) {
	int8_t one = 1;
	int8_t zero = 0;
	
	
	if (action == 1) {
		if (inodes[id].direct1 != FREE) {
			fseek(fs, sb->bitmap_start_address + inodes[id].direct1, SEEK_SET);
			fwrite(&one, sizeof(int8_t), 1, fs);
		}
		if (inodes[id].direct2 != FREE) {
			fseek(fs, sb->bitmap_start_address + inodes[id].direct2, SEEK_SET);
			fwrite(&one, sizeof(int8_t), 1, fs);
		}
		if (inodes[id].direct3 != FREE) {
			fseek(fs, sb->bitmap_start_address + inodes[id].direct3, SEEK_SET);
			fwrite(&one, sizeof(int8_t), 1, fs);
		}
		if (inodes[id].direct4 != FREE) {
			fseek(fs, sb->bitmap_start_address + inodes[id].direct4, SEEK_SET);
			fwrite(&one, sizeof(int8_t), 1, fs);
		}
		if (inodes[id].direct5 != FREE) {
			fseek(fs, sb->bitmap_start_address + inodes[id].direct5, SEEK_SET);
			fwrite(&one, sizeof(int8_t), 1, fs);
		}
		
		// + neprimy odkazy
		
	}
	else {
		fseek(fs, sb->bitmap_start_address + inodes[id].direct1, SEEK_SET);
		fwrite(&zero, sizeof(int8_t), 1, fs);
		fseek(fs, sb->bitmap_start_address + inodes[id].direct2, SEEK_SET);
		fwrite(&zero, sizeof(int8_t), 1, fs);
		fseek(fs, sb->bitmap_start_address + inodes[id].direct3, SEEK_SET);
		fwrite(&zero, sizeof(int8_t), 1, fs);
		fseek(fs, sb->bitmap_start_address + inodes[id].direct4, SEEK_SET);
		fwrite(&zero, sizeof(int8_t), 1, fs);
		fseek(fs, sb->bitmap_start_address + inodes[id].direct5, SEEK_SET);
		fwrite(&zero, sizeof(int8_t), 1, fs);
		
		// + neprimy odkazy
	}
	
	fflush(fs);
}

/* 	Update specific i-node in the file

	param id ... i-node id = offset in the file from the start of i-nodes
*/
void update_inode(int id) {
	fseek(fs, sb->inode_start_address + id * INODE_SIZE, SEEK_SET);
	
	fwrite(&(inodes[id].nodeid), sizeof(int32_t), 1, fs);
	fwrite(&(inodes[id].isDirectory), sizeof(int8_t), 1, fs);	
	fwrite(&(inodes[id].references), sizeof(int8_t), 1, fs);	
	fwrite(&(inodes[id].file_size), sizeof(int32_t), 1, fs);
	fwrite(&(inodes[id].direct1), sizeof(int32_t), 1, fs);
	fwrite(&(inodes[id].direct2), sizeof(int32_t), 1, fs);
	fwrite(&(inodes[id].direct3), sizeof(int32_t), 1, fs);
	fwrite(&(inodes[id].direct4), sizeof(int32_t), 1, fs);
	fwrite(&(inodes[id].direct5), sizeof(int32_t), 1, fs);
	fwrite(&(inodes[id].indirect1), sizeof(int32_t), 1, fs);
	fwrite(&(inodes[id].indirect2), sizeof(int32_t), 1, fs);
	
	fflush(fs);
}

int update_directory(int32_t id, directory_item *item, int action) {
	int i, j, counter = 0;
	int32_t *data_block;
	int name_length = 12;
	int zeros[4] = {0};  // buffer with zeros - for removing the item from the file
	int inode_count = 64;
	int blocks[5];
	
	int32_t nodeid;
	
	// Store numbers of data blocks
	if (inodes[id].direct1 != FREE) {
		blocks[0] = inodes[id].direct1;
		counter++;
		if (inodes[id].direct2 != FREE) {
			blocks[0] = inodes[id].direct2;
			counter++;
			if (inodes[id].direct3 != FREE) {
				blocks[0] = inodes[id].direct3;
				counter++;
				if (inodes[id].direct4 != FREE) {
					blocks[0] = inodes[id].direct4;
					counter++;
					if (inodes[id].direct5 != FREE) {
						blocks[0] = inodes[id].direct5;
						counter++;
						
						// + precist datove bloky z neprimych odkazu
					}
				}
			}
		}
	}

	if (action == 1) {	// Store item (find free space)
		for (i = 0; i < counter; i++) {
			fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
			for (j = 0; j < inode_count; j++) {
				fread(&nodeid, sizeof(int32_t), 1, fs);
				if (nodeid == 0) {	// Free place found -> store item
					fseek(fs, -4, SEEK_CUR);
					fflush(fs);
					fwrite(&(item->inode), sizeof(int32_t), 1, fs);
					fwrite(item->item_name, sizeof(item->item_name), 1, fs);
					fflush(fs);
					return NO_ERROR;
				}
				else {
					fseek(fs, name_length, SEEK_CUR);
				}
			}
		}
		
		data_block = find_free_data_blocks(1);
		if (data_block == NULL) 
			return ERROR;
		
		
		if (counter < 5) {	// Next direct reference is free
			switch(counter) {
				case 1:	
					inodes[id].direct2 = data_block[0];
					break;
				case 2:	
					inodes[id].direct3 = data_block[0];
					break;
				case 3:	
					inodes[id].direct4 = data_block[0];
					break;
				case 4:	
					inodes[id].direct5 = data_block[0];
					break;
			}
			fflush(fs);
			fseek(fs, sb->data_start_address + data_block[0] * CLUSTER_SIZE, SEEK_SET);
			fwrite(&(item->inode), sizeof(int32_t), 1, fs);
			fwrite(item->item_name, sizeof(item->item_name), 1, fs);
		}
		else {
			// + pouzit neprimy odkazy
		}
		free(data_block);
	}
	else {	// Remove item (find the item with the specific id)
		for (i = 0; i < counter; i++) {
			fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
			for (j = 0; j < inode_count; j++) {
				fread(&nodeid, sizeof(int32_t), 1, fs);
				if (nodeid == (item->inode)) {
					fseek(fs, -4, SEEK_CUR);
					fflush(fs);
					fwrite(&zeros, sizeof(zeros), 1, fs);
					fflush(fs);
					return NO_ERROR;
				}			
			}
		}
	}
	
	free(data_block);
	return ERROR;
}



