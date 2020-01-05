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
#include <math.h>
#include <string.h>
#include <limits.h>

#define BUFF_SIZE 256				// Buffer size for input commands
#define CLUSTER_SIZE 1024
#define INODE_SIZE 38			// Size of the i-node in bytes
#define MAX_NUMBERS_IN_BLOCK 256
#define MAX_SIZE 529408				// Maximum size of the file which can be stored in the filesystem (517 * 1024)
#define ERROR -1
#define NO_ERROR 0

#define FNF "FILE NOT FOUND\n"
#define PNF "PATH NOT FOUND\n"
#define TL "FILE IS TOO LARGE"
#define EXIST "EXIST\n"
#define NE "NOT EMPTY\n"
#define OK "OK\n"
#define CCF "CANNOT CREATE FILE\n"
#define NES "FILESYSTEM HAS NOT ENOUGH SPACE\n"

// Structure of supeblock
struct superblock {
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

// Structure of i-node
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

// Structure of directory item
typedef struct thedirectory_item{
    int32_t inode;               	// i-node ID (index to array)
    char item_name[12];             // File name 8+3 + \0
	struct thedirectory_item *next;	// Reference to another directory item in the current directory 
} directory_item;

// Structure of directory
typedef struct thedirectory {
	struct thedirectory *parent;	// Reference to the parent directory
	directory_item *current;		// Current directory item
	directory_item *subdir;			// Reference to the first subdirectory in the list of all subdirectories in the current directory
	directory_item *file;			// Reference to the first file in the list of all files in the current directory
} directory;	

// Structure of info. about particular data block
typedef struct thedata_info {
	int32_t nodeid;				// I-node id which contains this data block
	int32_t *ref_addr;			// Address of the direct/indirect reference where this data block is stored
								// NULL - if this data block is only in the block referenced by indirect reference
	int32_t indir_block;		// number of data block of indirect reference
	int32_t order_in_block;		// Location in the indirect data block (order of number)
} data_info;


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

int is_sorted(int32_t *blocks, int count);
data_info *create_data_info(int32_t nodeid, int32_t *ref_addr, int32_t indir_block, int32_t order_in_block);
data_info **map_data_blocks(int *count_of_full_blocks);
void switch_blocks(int from, int to, data_info **info_blocks);

int32_t get_size(char *size);
int32_t find_free_inode();
int32_t *find_free_data_blocks(int count);
int parse_path(char *path, char **name, directory **dir);
directory_item *find_item(directory_item *first_item, char *name);
int32_t *get_data_blocks(int32_t nodeid, int *block_count, int *rest);
int create_directory(directory *parent, char *name);
int test_existence(directory *dir, char *name);
directory_item *create_directory_item(int32_t inode_id, char *name);
directory *find_directory(char *path);
void initialize_inode(int32_t id, int32_t size, int block_count, int tmp_count, int *last_block_index, int32_t *blocks);
void free_directories(directory *root);
void clear_inode(int id);
void update_sizes(directory *dir, int32_t size);
void print_info(directory_item *item);
void print_file(directory_item *item);
void print_format_msg();

void load_fs();
void load_directory(directory *dir, int id);
void update_bitmap(directory_item *item, int8_t value, int32_t *data_blocks, int b_count);
void update_inode(int id);
int update_directory(directory *dir, directory_item *item, int action);
void remove_reference(directory_item *item, int32_t block_id);

const int32_t FREE = -1;					// item is free
const char *DELIM = " \n"; 

char *fs_name;							// Filesystem name
FILE *fs;								// File with filesystem
struct superblock *sb;					// Superblock
int8_t *bitmap = NULL;					// Bitmap of data blocks, 0 = free	1 = full
inode *inodes = NULL;					// Array of i-nodes, i-node ID = index to array
directory **directories = NULL;			// Array of pointers to directories, i-node ID = index to array
directory *working_directory;			// Current directory
int fs_formatted;						// If filesystem is formatted, 0 = false, 1 = true
char block_buffer[CLUSTER_SIZE];		// Buffer for one cluster 
int file_input = 0;						// If commands are loaded from a file


/* 	***************************************************
	Entry point of the program	
	
	param argv[1] ... name of the filesystem
*/
int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("No argument! Enter the filesystem name.\n");
		return EXIT_FAILURE;
	}
	
	printf("Filesystem is running...\n");
	fs_name = argv[1];
	
	// Test if filesystem already exists
	if (access(argv[1], F_OK) == -1) {
		fs_formatted = 0;
		printf("The filesystem has to be formatted first.\nUsage: format [size]\n");
	}
	else {
		fs_formatted = 1;
		load_fs();
	}
	
	run();
	shutdown();
	
	return EXIT_SUCCESS;
}


/*	Process user commands */
int run() {
	short exit = 0;	
	char buffer[BUFF_SIZE];	// User commands buffer
	char *cmd, *args;
	FILE *f;				// File from which can be loaded commands instead of console
	int32_t fs_size;		// Size of the filesystem
	
	do {
		memset(buffer, 0, BUFF_SIZE);

		if (file_input) {		// Commands from the file
			fgets(buffer, BUFF_SIZE, f);
			if (!feof(f)) {
				printf("%s", buffer);
			}
			else { 				// Switch back to the commands from the console
				file_input = 0;
				fclose(f);
				continue;
			}
		}
		else {					// Commands from the console
			fgets(buffer, BUFF_SIZE, stdin);
		}
		
		if (buffer[0] == '\n')	// Skip an empty line
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
			fs_size = get_size(buffer + strlen(cmd) + 1);
			if (fs_size == ERROR)		// Problem with the size of the filesystem
				continue;
			format(fs_size);
		}
		else if (strcmp("defrag", cmd) == 0) {
			defrag();
		}
		else if (buffer[0] == 'q') {	// Exiting command
			exit = 1;
		}
		else {
			printf("UNKNOWN COMMAND\n");
		}
	} while (!exit);
}

/* Perform all needed operations before exiting the program */
void shutdown() {
	if (sb) free(sb);
	if (bitmap) free(bitmap);
	if (inodes) free(inodes);
	if (directories) {
		free_directories(directories[0]);
		free(directories);
	}
	fclose(fs);
}


/*	Copy file to another directory

	param files ... source file (+path) and destination directory (+path)
*/
void cp(char *files) {
	int i, block_count, rest, count_with_indir, tmp, last_block_index;
	int32_t *source_blocks, *dest_blocks, inode_id;
	char *source, *dest, *name;
	directory *source_dir, *dest_dir;
	directory_item *item, **pitem;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (!files || files == "") { // No arguments
		printf(FNF);
		return;
	}
	source = strtok(files, " ");	// Get source
	dest = strtok(NULL, "\n");		// Get destination
	if (!dest || dest == "") {
		printf(FNF);
		return;
	}
	
	// Parse the source path + find the source directory
	if (parse_path(source, &name, &source_dir)) {
		printf(FNF);
		return;
	}
	
	// Find the file in the source directory
	item = find_item(source_dir->file, name);
	if (!item) {
		printf(FNF);
		return;
	}
	
	// Find the destination directory
	dest_dir = find_directory(dest);
	if (!dest_dir) {
		printf(PNF);
		return;
	}
	
	// Test if destination folder doesn't contain file/directory with the same name
	if (test_existence(dest_dir, name)) {
		printf(EXIST);
		return;
	}
	
	// Get numbers of data blocks of the source file
	source_blocks = get_data_blocks(item->inode, &block_count, &rest);
	
	if (block_count < 5)								// Use only direct references
		count_with_indir = block_count;
	else if ((block_count > 5) && (block_count < 262))	// Use first indirect reference (+1 data block)
		count_with_indir = block_count + 1;
	else 
		count_with_indir = block_count + 2;				// Use both indirect references (+2 data block)
	
	// Get numbers of free data blocks for copied file 
	dest_blocks = find_free_data_blocks(count_with_indir);
	if (!dest_blocks) {
		printf(NES);
		return;
	}
	
	// Get ID of a free i-node
	inode_id = find_free_inode();
	if (inode_id == ERROR) {
		printf(NES);
		return;
	}
	
	// Get the last (free) item in the list of all files in the destination directory
	pitem = &(dest_dir->file);
	while (*pitem != NULL) {
		pitem = &((*pitem)->next);
	}
	*pitem = create_directory_item(inode_id, name);
	
	// Initialize i-node
	initialize_inode(inode_id, inodes[item->inode].file_size, block_count, count_with_indir, &last_block_index, dest_blocks);

	// Save changes to the file
	update_bitmap(*pitem, 1, dest_blocks, block_count);
	update_inode(inode_id);
	update_directory(dest_dir, *pitem, 1);
	update_sizes(dest_dir, inodes[item->inode].file_size);
	
	// Copy data blocks
	for (i = 0; i < block_count - 1; i++) {
		fseek(fs, sb->data_start_address + source_blocks[i] * CLUSTER_SIZE, SEEK_SET);
		fflush(fs);
		fread(block_buffer, sizeof(block_buffer), 1, fs);
		fseek(fs, sb->data_start_address + dest_blocks[i] * CLUSTER_SIZE, SEEK_SET);
		fflush(fs);
		fwrite(block_buffer, sizeof(block_buffer), 1, fs);
	}
	
	// Copy the last data block (may copy only a part of the block)
	memset(block_buffer, 0, CLUSTER_SIZE);
	if (rest != 0)
		tmp = rest;
	else 
		tmp = CLUSTER_SIZE;
	
	fseek(fs, sb->data_start_address + source_blocks[block_count - 1] * CLUSTER_SIZE, SEEK_SET);
	fflush(fs);
	fread(block_buffer, tmp, 1, fs);
	fseek(fs, sb->data_start_address + dest_blocks[last_block_index] * CLUSTER_SIZE, SEEK_SET);
	fflush(fs);
	fwrite(block_buffer, tmp, 1, fs);
	fflush(fs);
	
	free(source_blocks);
	free(dest_blocks);
	
	printf(OK);	
}


/*	Move file to another directory

	param files ... source file (+path) and destination directory (+path)
*/
void mv(char *files) {
	char *source, *dest, *name;
	directory *source_dir, *dest_dir;
	directory_item *item, **pitem2, **temp;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (!files || files == "") { 	// No arguments
		printf(FNF);
		return;
	}
	source = strtok(files, " ");	// Get source
	dest = strtok(NULL, "\n");		// Get destination
	if (!dest || dest == "") {
		printf(FNF);
		return;
	}
	
	// Parse the source path + find the source directory
	if (parse_path(source, &name, &source_dir)) {
		printf(FNF);
		return;
	}
	
	// Find the destination directory
	dest_dir = find_directory(dest);
	if (!dest_dir) {
		printf(PNF);
		return;
	}
	
	// If source and destination directories are the same
	if (dest_dir == source_dir) {
		printf(OK);
		return;
	}
	
	// Test if destination folder doesn't contain file/directory with the same name
	if (test_existence(dest_dir, name)) {
		printf(EXIST);
		return;
	}
	
	// Remove the file from the list of all files in the source directory
	temp = &(source_dir->file); 
	item = source_dir->file;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			(*temp) = item->next;
			update_directory(source_dir, item, 0);
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
	

	// Get the last (free) item in the list of all files in the destination directory
	pitem2 = &(dest_dir->file);
	while (*pitem2 != NULL) {
		pitem2 = &((*pitem2)->next);
	}
	
	*pitem2 = item;	// Add file to the destination directory
	
	update_directory(dest_dir, item, 1);
	update_sizes(dest_dir, inodes[item->inode].file_size);
	
	printf(OK);
}


/*	Remove file

	param file ... removing file (+path)
*/
void rm(char *file) {
	int i, block_count, rest, tmp, prev;
	int32_t *blocks;
	char *name;
	directory *dir;
	directory_item *item, **temp;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (!file || file == "") {
		printf(FNF);
		return;
	}
	
	// Parse the path + find the directory
	if (parse_path(file, &name, &dir)) {
		printf(FNF);
		return;
	}

	// Remove the file from the list of all files in the directory
	temp = &(dir->file);
	item = dir->file;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			(*temp) = item->next;
			break;
		}
		temp = &(item->next);
		item = item->next;
	}
	
	if (!item) {
		printf(FNF);
		return;
	}

	// Get numbers of data blocks of the file
	blocks = get_data_blocks(item->inode, &block_count, &rest);

	// Clear data blocks
	memset(block_buffer, 0, CLUSTER_SIZE);
	prev = blocks[0];
	for (i = 0; i < block_count - 1; i++) {
		if (prev != blocks[i] - 1) {
			fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
		}
		fwrite(block_buffer, sizeof(block_buffer), 1, fs);
		prev = blocks[i];
	}
	
	if (rest != 0)
		tmp = rest;
	else 
		tmp = CLUSTER_SIZE;
	
	fseek(fs, sb->data_start_address + blocks[block_count - 1] * CLUSTER_SIZE, SEEK_SET);
	fwrite(block_buffer, tmp, 1, fs);
	
	if (inodes[item->inode].indirect1 != FREE) {
		fseek(fs, sb->data_start_address + inodes[item->inode].indirect1 * CLUSTER_SIZE, SEEK_SET);
		fwrite(block_buffer, sizeof(block_buffer), 1, fs);
		
		if (inodes[item->inode].indirect2 != FREE) {
			fseek(fs, sb->data_start_address + inodes[item->inode].indirect2 * CLUSTER_SIZE, SEEK_SET);
			fwrite(block_buffer, sizeof(block_buffer), 1, fs);
		}
	}
	
	fflush(fs);

	update_bitmap(item, 0, blocks, block_count);
	update_sizes(dir, -(inodes[item->inode].file_size));
	update_directory(dir, item, 0);
	
	clear_inode(item->inode);
	update_inode(item->inode);
	
	free(item);
	free(blocks);
	
	printf(OK);
}


/*	Create a new directory
	
	param path ... name of the new directory (+path)
*/
void mymkdir(char *path) {
	directory *dir;
	char *name;			
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (!path || path == "") {
		printf(PNF);
		return;
	}
	
	// Parse the path + find the parent directory
	if (parse_path(path, &name, &dir)) {
		printf(PNF);
		return;
	}

	// Test if destination folder doesn't contain file/directory with the same name
	if (test_existence(dir, name)) {
		printf(EXIST);
		return;
	}

	// Create directory
	if (create_directory(dir, name)) {
		printf(NES);
		return;
	}
	printf(OK);
}


/* 	Remove the empty directory

	param path ... name of the removing directory (+path)
*/
void myrmdir(char *path) {
	directory *dir;
	directory_item *item, **temp;
	char *name;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (!path || path == "") {
		printf(PNF);
		return;
	}
	
	// Parse the path + find the parent directory
	if (parse_path(path, &name, &dir)) {
		printf(PNF);
		return;
	}

	temp = &(dir->subdir); 
	item = dir->subdir;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			if ((directories[item->inode]->file != NULL) || (directories[item->inode]->subdir != NULL)) {		// If directory is not empty
				printf(NE);		
				return;			
			}

			(*temp) = item->next;
			
			if (working_directory == directories[item->inode]) {	// If removing working directory -> get to one level up in hierarchy
				working_directory = directories[item->inode]->parent;
			}
			
			update_bitmap(item, 0, NULL, 0);
			clear_inode(item->inode);
			update_inode(item->inode);
			update_directory(dir, item, 0);

			free(directories[item->inode]);
			free(item);
			break;
		}
		temp = &(item->next);
		item = item->next;
	}
	
	if (!item) {	// If directory wasn't found
		printf(FNF);
		return;
	}

	printf(OK);
}


/*	Print all items in the directory

	param path ... path of the directory
*/
void ls(char *path) {
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
	
	// Find the directory
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


/*	Print content of the file

	param file ... name of file (+path)
*/
void cat(char *file) {
	directory *dir;
	directory_item *item;
	char *name;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (!file || file == "") {
		printf(FNF);
		return;
	}
	
	// Parse the path + file + find the directory
	if (parse_path(file, &name, &dir)) {
		printf(FNF);
		return;
	}
	
	item = find_item(dir->file, name);
	if (!item) {
		printf(FNF);
		return;
	}
	print_file(item);
	printf("\n");
}


/*	Change the working directory according to the path

	param path ... path to the new working directory
*/
void cd(char *path) {
	directory *dir;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (!path || path == "") {
		printf(PNF);
		return;
	}
	
	// Find the directory
	dir = find_directory(path);
	if (!dir) {		
		printf(PNF);
		return;
	}
	
	working_directory = dir;
	printf(OK);
}


/*	Print the path of the working directory */
void pwd() {
	char *names[64];	// Directory names on the path to the working directory
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


/*	Print the info about the file/directory

	param path ... file/directory name (+path)
*/
void info(char *path) {
	directory *dir;
	char *name;
	directory_item *item;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (!path || path == "") {
		printf(FNF);
		return;
	}
	
	// Parse the path + file + find the directory
	if (parse_path(path, &name, &dir)) {
		printf(FNF);
		return;
	}
	
	// If directory is root
	if (dir == directories[0] && strlen(name) == 0) {
		print_info(dir->current);
		return;
	}
	
	// Finding item between files 
	if (item = find_item(dir->file, name)) {
		print_info(item);
		return;
		
	}
	
	// Finding item between subdirectories
	if (item = find_item(dir->subdir, name)) {
		print_info(item);
		return;
	}
	
	printf(FNF);
}


/*	Copy file from the extern filesystem into this filesystem

	param files ... source file (+path) and destination directory (+path)
*/
void incp(char *files) {
	int32_t file_size, *blocks, inode_id;
	int i, block_count, rest, tmp_count, tmp, last_block_index, prev;
	char *source, *dest, *name;
	directory *dir; 
	directory_item **pitem;
	FILE *f;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (!files || files == "") { // No arguments
		printf(FNF);
		return;
	}
	source = strtok(files, " ");	// Get source
	dest = strtok(NULL, "\n");		// Get destination
	if (!dest || dest == "") {
		printf(PNF);
		return;
	}
	
	// Get name of the source file
	if ((name = strrchr(source, '/')) == NULL) {
		name = source;
	}
	else {
		name++;
	}
	
	// Find destination directory
	dir = find_directory(dest);
	if (!dir) {
		printf(PNF);
		return;
	}
	
	// Test if destination folder doesn't contain file with the same name
	if (test_existence(dir, name)) {
		printf(EXIST);
		return;
	}
	
	if (!(f = fopen(source, "rb"))) {
		printf(FNF);
		return;
	}
	
	// Get size of the copied file
	fseek(f, 0, SEEK_END);
	file_size = ftell(f);
	rewind(f);
	
	if (file_size > MAX_SIZE) {
		printf(TL);
		fclose(f);
		return;
	}
	
	block_count = file_size / CLUSTER_SIZE;
	rest = file_size % CLUSTER_SIZE;
	
	if (rest != 0)
		block_count++;
	
	if (block_count < 5)	// Use only direct references
		tmp_count = block_count;
	else if ((block_count > 5) && (block_count < 262))	// Use first indirect reference (+1 data block)
		tmp_count = block_count + 1;
	else 
		tmp_count = block_count + 2;			// Use both indirect references (+2 data block)
	
	blocks = find_free_data_blocks(tmp_count);
	if (!blocks) {
		printf(NES);
		fclose(f);
		return;
	}
	
	// Get ID of a free i-node
	inode_id = find_free_inode();
	if (inode_id == ERROR) {
		printf(NES);
		fclose(f);
		return;
	}
	
	pitem = &(dir->file);
	while (*pitem != NULL) {
		pitem = &((*pitem)->next);
	}
	*pitem = create_directory_item(inode_id, name);

	// Initialize i-node
	initialize_inode(inode_id, file_size, block_count, tmp_count, &last_block_index, blocks);

	update_bitmap(*pitem, 1, blocks, block_count);
	update_inode(inode_id);
	update_directory(dir, *pitem, 1);
	update_sizes(dir, file_size);
	
	// Copy data
	prev = blocks[0];
	for (i = 0; i < block_count - 1; i++) {
		fread(block_buffer, sizeof(block_buffer), 1, f);
		if (prev != blocks[i] - 1) {
			fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
		}
		fwrite(block_buffer, sizeof(block_buffer), 1, fs);
		prev = blocks[i];
	}
	
	memset(block_buffer, 0, CLUSTER_SIZE);
	if (rest != 0)
		tmp = rest;
	else 
		tmp = CLUSTER_SIZE;
	
	fread(block_buffer, sizeof(char), tmp, f);
	fseek(fs, sb->data_start_address + blocks[last_block_index] * CLUSTER_SIZE, SEEK_SET);
	fwrite(block_buffer, sizeof(char), tmp, fs);
	fflush(fs);
	
	fclose(f);
	free(blocks);

	printf(OK);
}


/*	Copy file from this filesystem to extern filesystem

	param files ... source file (+path) and destination directory (+path)
*/
void outcp(char *files) {
	int i, block_count, rest, tmp, prev;
	int32_t *blocks;
	char *source, *dest, *name;
	char whole_dest[BUFF_SIZE];
	directory *dir;
	directory_item *item;
	FILE *f;
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}
	
	if (!files || files == "") { // No arguments
		printf(FNF);
		return;
	}
	source = strtok(files, " ");	// Get source
	dest = strtok(NULL, "\n");		// Get destination
	if (!dest || dest == "") {
		printf(PNF);
		return;
	}
	// Parse the path + file + find the directory
	if (parse_path(source, &name, &dir)) {
		printf(FNF);
		return;
	}
	
	// Set a whole destination (path + name)
	memset(whole_dest, 0, BUFF_SIZE);
	sprintf(whole_dest, "%s/%s", dest, name);
	
	if (!(f = fopen(whole_dest, "wb"))) {
		printf(PNF);
		return;
	}
	
	item = find_item(dir->file, name);
	
	if (!item) {
		printf(FNF);
		return;
	}
	
	blocks = get_data_blocks(item->inode, &block_count, &rest);

	// Copy data
	prev = blocks[0];
	for (i = 0; i < block_count - 1; i++) {
		if (prev != (blocks[i] - 1)) {	// If data blocks are not in sequence
			fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
		}
		fread(block_buffer, sizeof(block_buffer), 1, fs);
		fwrite(block_buffer, sizeof(block_buffer), 1, f);
		fflush(f);
		prev = blocks[i];
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


/*	Load a file with commands to perform

	param file ... name of the file with commands (+path)
	return open file
*/
FILE *load(char *file) {
	FILE *f;
	
	if (!fs_formatted) {
		print_format_msg();
		return NULL;	
	}

	if ((f = fopen(file, "r")) == NULL) {
		printf(FNF);
		return NULL;
	}
	file_input = 1;	// Set the indicator that commands are loaded from the file
	
	printf(OK);
	return f;
}


/* 	Format existing filesystem or create a new one with a specific size

	param bytes ... size of the filesystem in bytes
*/
void format(long bytes) {
	int i, one = 1;
	directory *root;
	
	if (!fs) {
		fs = fopen(fs_name, "wb+");
	}
	
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
	
	
//	printf("Size: %d\nCount of clusters: %d\nCount of i-nodes: %d\nCount of bitmap blocks: %d\nCount of i-node blocks: %d\nCount of data blocks: %d\nAddress of bitmap: %d\nAddress of i-nodes: %d\nAddress of data: %d\n", 
//	sb->disk_size, sb->cluster_count, sb->inode_count, sb->bitmap_cluster_count, sb->inode_cluster_count, sb->data_cluster_count, sb->bitmap_start_address, sb->inode_start_address, sb->data_start_address);
	
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
	
	// Store bitmap - data block 0 (root)
	fseek(fs, sb->bitmap_start_address, SEEK_SET);
	fwrite(&one, sizeof(int8_t), 1, fs);
	
	// Store i-nodes
	for (i = 0; i < sb->inode_count; i++) {
		update_inode(i);
	}
	
	fs_formatted = 1;
	printf(OK);
}


/*	Defragment filesystem - first shift all data blocks to the begin of file (remove spaces between data blocks)
	and then reorder not consecutive data blocks belonging to one i-node
*/
void defrag() {
	int32_t i, j, k, l, tmp, rest, count_of_full_blocks = 0;
	int32_t *blocks, *blocks2;
	short changed_inodes[sb->inode_count];	// bitmap of i-nodes which were modified	1 = changed, 0 = unchanged
	int inode_block_count[sb->inode_count];	// count of data blocks for every i-node
	int32_t **data_blocks;					// array of data blocks for every i-node
	data_info **info_blocks;				// array of information for every full data block
	
	if (!fs_formatted) {
		print_format_msg();
		return;	
	}

	// Prepare data for defragmentation
	info_blocks = map_data_blocks(&count_of_full_blocks);
	data_blocks = (int32_t **)malloc(sizeof(int32_t *) * sb->inode_count);

	for (i = 0; i < sb->inode_count; i++) {
		data_blocks[i] = NULL;
	}
	for (i = 0; i < sb->inode_count; i++) {
		if (inodes[i].nodeid == FREE) {
			continue;
		}

		data_blocks[i] = get_data_blocks(i, &(inode_block_count[i]), &rest);
		
		tmp = 0;
		if (inodes[i].indirect1 != FREE) 
			tmp++;
		if (inodes[i].indirect2 != FREE)
			tmp++;
			
		if (tmp != 0) {
			data_blocks[i] = (int32_t *)realloc(data_blocks[i], sizeof(int32_t) * inode_block_count[i] + tmp);
			if (tmp == 1) {
				data_blocks[i][inode_block_count[i]] = inodes[i].indirect1;
			}
			else {
				data_blocks[i][inode_block_count[i]] = inodes[i].indirect2;
				data_blocks[i][inode_block_count[i] + 1] = inodes[i].indirect1;
			}
			inode_block_count[i] += tmp;
		}
	}

	// Rearrange data blocks so that free blocks were after full blocks
	for (i = 0; i < count_of_full_blocks; i++) {
		if (info_blocks[i] != NULL) 
			continue;
		
		j = i + 1;
		// Find the next nearest full data block
		while (info_blocks[j] == NULL) {
			j++;
		}
		changed_inodes[info_blocks[j]->nodeid] = 1;
		switch_blocks(j, i, info_blocks);
	}

	// Rearrange data blocks so that blocks of one i-node were consecutive
	i = 0;
	while (i < count_of_full_blocks) {	// Iteration over full data blocks
		blocks = data_blocks[info_blocks[i]->nodeid];	// get all data blocks of the i-node to which current data block belongs
			
		if (is_sorted(blocks, inode_block_count[info_blocks[i]->nodeid])) {	// if data blocks are already sorted -> skip to another data block of other i-node
			i += inode_block_count[info_blocks[i]->nodeid];
			continue;
		}

		for (j = i, k = 0; j < (i + inode_block_count[info_blocks[i]->nodeid]); j++, k++) {
			if (j == blocks[k]) 
				continue;
			
			changed_inodes[info_blocks[j]->nodeid] = 1;
			changed_inodes[info_blocks[blocks[k]]->nodeid] = 1;
			
			blocks2 = data_blocks[info_blocks[j]->nodeid];	// data blocks of the destination
			for (l = 0; l < inode_block_count[info_blocks[j]->nodeid]; l++) {
				if (j == blocks2[l]) {
					blocks2[l] = blocks[k];
					break;
				}
			}
			switch_blocks(blocks[k], j, info_blocks);
			blocks[k] = j;
		}
		i += inode_block_count[info_blocks[i]->nodeid];
	}
	
	// Save bitmap
	fseek(fs, sb->bitmap_start_address, SEEK_SET);
	fwrite(bitmap, sizeof(int8_t), sb->data_cluster_count, fs);
	
	// Save changed i-nodes
	for (i = 0; i < sb->inode_count; i++) {
		if (changed_inodes[i] == 1) {
			update_inode(i);
		}
	}
	
	// Free data blocks
	for (i = 0; i < sb->inode_count; i++) {
		if (data_blocks[i] == NULL)
			continue;
		
		free(data_blocks[i]);
	}
	free(data_blocks);
	
	// Free data_info blocks
	for (i = 0; i < sb->data_cluster_count; i++) {
		if (info_blocks[i] == NULL)
			continue;
			
		free(info_blocks[i]);
	}
	free(info_blocks);
	
	printf(OK);
}


/*	Test if data blocks are consecutive

	param blocks ... data blocks
	param count ... count of blocks
	return 0 = unsorted, 1 = sorted
*/
int is_sorted(int32_t *blocks, int count) {
	int i;
	
	for (i = 1; i < count; i++) {
		if (blocks[i - 1] != (blocks[i] - 1)) {
			return 0;	
		}
	}
	return 1;
}


/*	Create a data info for the specific data block

	param nodeid ... i-node id to which belongs this data block
	param ref_addr ... address where is stored number of this data block (in i-node)
	param indir_block ... number of data block which is indirect reference
	param order_in_block ... if this data block is stored in the indirect reference -> order in that block
	return info about this data block
*/ 
data_info *create_data_info(int32_t nodeid, int32_t *ref_addr, int32_t indir_block, int32_t order_in_block) {
	data_info *block = (data_info *)malloc(sizeof(data_info));
	block->nodeid = nodeid;
	block->ref_addr = ref_addr;
	block->indir_block = indir_block;
	block->order_in_block = order_in_block;
	
	return block;
}


/*	Create an array of information for every full data block

	param count_of_full_blocks ... count of used data blocks
	return array of information
*/
data_info **map_data_blocks(int *count_of_full_blocks) {
	int i, j;
	int32_t number;
	data_info **blocks = (data_info **)malloc(sizeof(data_info *) * sb->data_cluster_count);
	for (i = 0; i < sb->data_cluster_count; i++) {
		blocks[i] = NULL;
	}
	
	for (i = 0; i < sb->inode_count; i++) {
		if (inodes[i].nodeid == FREE)
			continue;
			
		if (inodes[i].direct1 != FREE) {
			blocks[inodes[i].direct1] = create_data_info(inodes[i].nodeid, &(inodes[i].direct1), 0, 0);
			(*count_of_full_blocks)++;
		}
		if (inodes[i].direct2 != FREE) {
			blocks[inodes[i].direct2] = create_data_info(inodes[i].nodeid, &(inodes[i].direct2), 0, 0);
			(*count_of_full_blocks)++;
		}
		if (inodes[i].direct3 != FREE) {
			blocks[inodes[i].direct3] = create_data_info(inodes[i].nodeid, &(inodes[i].direct3), 0, 0);
			(*count_of_full_blocks)++;
		}
		if (inodes[i].direct4 != FREE) {
			blocks[inodes[i].direct4] = create_data_info(inodes[i].nodeid, &(inodes[i].direct4), 0, 0);
			(*count_of_full_blocks)++;
		}
		if (inodes[i].direct5 != FREE) {
			blocks[inodes[i].direct5] = create_data_info(inodes[i].nodeid, &(inodes[i].direct5), 0, 0);
			(*count_of_full_blocks)++;
		}
		if (inodes[i].indirect1 != FREE) {
			blocks[inodes[i].indirect1] = create_data_info(inodes[i].nodeid, &(inodes[i].indirect1), inodes[i].indirect1, 0);
			(*count_of_full_blocks)++;
			fseek(fs, sb->data_start_address + inodes[i].indirect1 * CLUSTER_SIZE, SEEK_SET);
			for (j = 0; j < MAX_NUMBERS_IN_BLOCK; j++) {
				fread(&number, sizeof(int32_t), 1, fs);
				if (number > 0) {
					blocks[number] = create_data_info(inodes[i].nodeid, NULL, inodes[i].indirect1, j);
					(*count_of_full_blocks)++;
				}
			}
		}
		if (inodes[i].indirect2 != FREE) {
			blocks[inodes[i].indirect2] = create_data_info(inodes[i].nodeid, &(inodes[i].indirect2), inodes[i].indirect2, 0);
			(*count_of_full_blocks)++;
			fseek(fs, sb->data_start_address + inodes[i].indirect2 * CLUSTER_SIZE, SEEK_SET);
			for (j = 0; j < MAX_NUMBERS_IN_BLOCK; j++) {
				fread(&number, sizeof(int32_t), 1, fs);
				if (number > 0) {
					blocks[number] = create_data_info(inodes[i].nodeid, NULL, inodes[i].indirect2, j);
					(*count_of_full_blocks)++;
				}
			}
		}
	}
	
	fflush(fs);
	return blocks;
}


/*	Switch two data blocks

	param from ... number of the first data block
	param to ... number of the second data block
	param info_blocks ... array of information of all data blocks
*/ 
void switch_blocks(int from, int to, data_info **info_blocks) {
	int i;
	int32_t number;
	data_info *tmp;
	char tmp_buffer[CLUSTER_SIZE];

	// Update source (from) i-node
	if (info_blocks[from]->ref_addr != NULL) {
		if (info_blocks[from]->indir_block > 0) {		// indirect reference -> change data block number to all blocks in this indirect reference
			fseek(fs, sb->data_start_address + info_blocks[from]->indir_block * CLUSTER_SIZE, SEEK_SET);
			for (i = 0; i < MAX_NUMBERS_IN_BLOCK; i++) {
				fread(&number, sizeof(int32_t), 1, fs);
				if (number > 0) {
					info_blocks[number]->indir_block = to;
				}
			}
			fflush(fs);
			info_blocks[from]->indir_block = to;
		}
		*(info_blocks[from]->ref_addr) = to; 
		
	}
	else {	// is in the data block of indirect reference	
		fseek(fs, sb->data_start_address + info_blocks[from]->indir_block * CLUSTER_SIZE + info_blocks[from]->order_in_block * sizeof(int32_t), SEEK_SET);
		fwrite(&to, sizeof(int32_t), 1, fs);
		fflush(fs);
	}
	
	if (info_blocks[to] == NULL) { // Destination data block is free -> one directional move
		// Update bitmap
		bitmap[from] = 0;
		bitmap[to] = 1;
		
		memset(tmp_buffer, 0, sizeof(tmp_buffer));
		
		info_blocks[to] = info_blocks[from];
		info_blocks[from] = NULL;
	}
	else  {	// Exchange of data blocks
		
		// Update destination (to) i-node
		if (info_blocks[to]->ref_addr != NULL) {
			if (info_blocks[to]->indir_block > 0) {		// indirect reference -> change data block number to all blocks in this indirect reference
				fseek(fs, sb->data_start_address + *(info_blocks[to]->ref_addr) * CLUSTER_SIZE, SEEK_SET);
				for (i = 0; i < MAX_NUMBERS_IN_BLOCK; i++) {
					fread(&number, sizeof(int32_t), 1, fs);
					if (number > 0) {
						info_blocks[number]->indir_block = from;
					}
				}
				fflush(fs);
				info_blocks[to]->indir_block = from;
			}
			*(info_blocks[to]->ref_addr) = from; 
		}
		else {		
			fseek(fs, sb->data_start_address + info_blocks[to]->indir_block * CLUSTER_SIZE + info_blocks[to]->order_in_block * sizeof(int32_t), SEEK_SET);
			fwrite(&from, sizeof(int32_t), 1, fs);
			fflush(fs);
		}
							
		//Copy: to -> tmp_buffer
		fseek(fs, sb->data_start_address + to * CLUSTER_SIZE, SEEK_SET);
		fread(tmp_buffer, sizeof(tmp_buffer), 1, fs);
		
		tmp = info_blocks[to];
		info_blocks[to] = info_blocks[from];
		info_blocks[from] = tmp;
	}
	
							// Copy blocks
	// from -> block_buffer
	fseek(fs, sb->data_start_address + from * CLUSTER_SIZE, SEEK_SET);
	fread(block_buffer, sizeof(block_buffer), 1, fs);
	// block_buffer -> to
	fseek(fs, sb->data_start_address + to * CLUSTER_SIZE, SEEK_SET);
	fflush(fs);
	fwrite(block_buffer, sizeof(block_buffer), 1, fs);
	// tmp_buffer -> from
	fseek(fs, sb->data_start_address + from * CLUSTER_SIZE, SEEK_SET);
	fwrite(tmp_buffer, sizeof(tmp_buffer), 1, fs);
	fflush(fs);
}


/*	Validate entered size of the filesystem
	and convert it into bytes
	
	param size ... size of the filesystem as the string
	return ... size of the filesystem in bytes
*/
int32_t get_size(char *size) {
	char *units = NULL;
	long number;
	
	if (!size || size == "") {
		printf(CCF);
		return ERROR;
	}
	
	number = strtol(size, &units, 0);	// Convert to number
	
	if (number == 0 || errno != 0) {
		printf(CCF);
		return ERROR;
	}
	
	if (strncmp("KB", units, 2) == 0) {			// Kilobytes
		number *= 1000;
	}
	else if (strncmp("MB", units, 2) == 0) {	// Megabytes
		number *= 1000000;
	}
	else if (strncmp("GB", units, 2) == 0) {	// Gigabytes
		number *= 1000000000;
	}
	
	if (number < 20480) {			// If the size is not enough large 
		printf(CCF);
		return ERROR;
	}
	else if (number > INT_MAX) {	// If the size is too large
		printf(CCF);
		return ERROR;
	}
	
	return (int32_t)number;
}


/*	Find a free i-node
	
	return ... i-node ID or -1 if no i-node is free
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


/* 	Find free data blocks in bitmap	

	param count ... count of data blocks
	return array of free data blocks or NULL if not enough blocks found
*/
int32_t *find_free_data_blocks(int count) {
	int i, j = 0;
	int32_t *blocks = (int32_t *)malloc(sizeof(int32_t) * count);
	
	// Try to find consecutive blocks
	for (i = 1; i < sb->data_cluster_count; i++) {
		if (bitmap[i] == 0) {
			if ((j != 0) && (blocks[j - 1] != (i - 1))) {
				j = 0;
				i--;
				continue;
			}
			blocks[j++] = i;
			if (j == count) {
				return blocks;
			}
		}
	}
	
	j = 0;
	// If blocks are not consecutive
	for (i = 1; i < sb->data_cluster_count; i++) {
		if (bitmap[i] == 0) {
			blocks[j] = i;
			j++;
			if (j == count)
				return blocks;
		}
	}
	free(blocks);
	return NULL;
}


/*	Get numbers of all data blocks of the particular item

	param item ... item from which we get data blocks
	param block_count ... address to store count of data blocks
	param rest ... address to store rest size of the last data block (only with file)
	return array of numbers of data blocks
*/
int32_t *get_data_blocks(int32_t nodeid, int *block_count, int *rest) {
	int32_t *blocks, number;
	int i, tmp, counter;
	int max_numbers = 517;	// Maximum data blocks 
	inode *node = &inodes[nodeid];
	
	if (node->isDirectory) {	// If item is directory
		counter = 0;
		blocks = (int32_t *)malloc(sizeof(int32_t) * max_numbers);
		
		if (node->direct1 != FREE) {
			blocks[counter++] = node->direct1;
		}
		if (node->direct2 != FREE) {
			blocks[counter++] = node->direct2;
		}
		if (node->direct3 != FREE) {
			blocks[counter++] = node->direct3;
		}	
		if (node->direct4 != FREE) {
			blocks[counter++] = node->direct4;
		}	
		if (node->direct5 != FREE) {
			blocks[counter++] = node->direct5;
		}	
		if (node->indirect1 != FREE) {
			fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
			for (i = 0; i < MAX_NUMBERS_IN_BLOCK; i++) {
				fread(&number, sizeof(int32_t), 1, fs);
				if (number > 0) {
					blocks[counter++] = number;
				}
			}
		}
		if (node->indirect2 != FREE) {
			fseek(fs, sb->data_start_address + node->indirect2 * CLUSTER_SIZE, SEEK_SET);
			for (i = 0; i < MAX_NUMBERS_IN_BLOCK; i++) {
				fread(&number, sizeof(int32_t), 1, fs);
				if (number > 0) {
					blocks[counter++] = number;
				}
			}			
		}
		
		*block_count = counter;
	}
	else {	// If item is file
		*block_count = node->file_size / CLUSTER_SIZE;
		*rest = node->file_size % CLUSTER_SIZE;
		if (*rest != 0)
			(*block_count)++;
		
		blocks = (int32_t *)malloc(sizeof(int32_t) * (*block_count));
		
		blocks[0] = node->direct1;
		if (*block_count > 1) {
			blocks[1] = node->direct2;
			if (*block_count > 2) {
				blocks[2] = node->direct3;
				if (*block_count > 3) {
					blocks[3] = node->direct4;
					if (*block_count > 4) {
						blocks[4] = node->direct5;
						if (*block_count > 5) {
							if (*block_count > 261) {	// Both indirect references are used
								// Read all data blocks of indirect1
								fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
								fread(&blocks[5], sizeof(int32_t), MAX_NUMBERS_IN_BLOCK, fs);
								
								// Read the rest of indirect2
								tmp = *block_count - 261;
								fseek(fs, sb->data_start_address + node->indirect2 * CLUSTER_SIZE, SEEK_SET);
								fread(&blocks[261], sizeof(int32_t), tmp, fs);
							}
							else {	// Only first indirect reference is used
								tmp = *block_count - 5;
								fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
								fread(&blocks[5], sizeof(int32_t), tmp, fs);
							}
						}
					}
				}
			}
		}
	}
	fflush(fs);
	return blocks;
}


/* 	Find particular item with the specific name in a list of items starting with first_item

	param first_item ... first item in the list of items (directories or files)
	param name ... name of the finding item
	return founded item or NULL
*/
directory_item *find_item(directory_item *first_item, char *name) {
	directory_item *item = first_item;
	
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			return item;
		}
		item = item->next;
	}
	return NULL;
}


/*	Parse the path of the file/directory

	param path ... path + name of file/directory
	param name ... address to store name of the file/directory
	param dir ... address to store founded directory
	return 0 = directory found, -1 = not found
*/
int parse_path(char *path, char **name, directory **dir) {
	int length;				// Length of the path (without name of the new directory)
	char buff[BUFF_SIZE];
	
	if (!path || path == "") {
		return ERROR;
	}
	
	if ((*name = strrchr(path, '/')) == NULL) {	// If path contains only directory/file name
		*name = path;
		*dir = working_directory;
	}
	else  {
		// Separate the name of the directory/file from the rest of the path
		length = strlen(path) - strlen(*name);
		if (path[0] == '/') {					
			if (!strchr(path + 1, '/'))	// If the path contains only root directory (and name of the creating directory)
				length = 1;
		}
		
		*name = *name + 1;
		memset(buff, '\0', BUFF_SIZE);
		strncpy(buff, path, length);
		
		// Find the directory
		*dir = find_directory(buff);		
		if (!(*dir)) {
			return ERROR;
		}
	}

	return NO_ERROR;
}



/*	Create a new directory
	
	param parent ... parent directory
	param name ... directory name
	return 0 = no error, -1 = error
*/
int create_directory(directory *parent, char *name) {
	int32_t inode_id, *data_block;
	directory_item **temp;

	// Get ID of a free i-node
	inode_id = find_free_inode();
	if (inode_id == ERROR) return ERROR;	// No free i-node
	
	// Get number of a free data block
	data_block = find_free_data_blocks(1);
	if (data_block == NULL) return ERROR; 	// No free data block

	// Create directory
	directory *newdir = (directory *)malloc(sizeof(directory));
	newdir->parent = parent;
	newdir->current = create_directory_item(inode_id, name);
	newdir->file = NULL;
	newdir->subdir = NULL;
	
	directories[inode_id] = newdir;
	bitmap[data_block[0]] = 1;
	
	// Initialize i-node of a new directory
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
	
	if (update_directory(parent, newdir->current, 1)) {
		return ERROR;	// No free data block for extending parent data blocks
	}

	update_inode(inode_id);
	update_bitmap(newdir->current, 1, data_block, 1);
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


/*	Test if directory already contains item with the same name

	param dir ... directory
	param name ... name of the testing item
	return 1 = exist, 0 = not exist
*/
int test_existence(directory *dir, char *name) {
	directory_item *item;
	
	// If the file with the same name already exist 
	item = dir->file;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {
			return 1;
		}
		item = item->next;
	}
	
	// If the directory with the same name already exist
	item = dir->subdir;
	while (item != NULL) {
		if (strcmp(name, item->item_name) == 0) {	
			return 1;
		}
		item = item->next;
	}
	return 0;
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


/*	Free allocated memory for directories */
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
		f = t;
	}
	
	free(d);
	d = NULL;
	free(root->current);
	free(root);
	root = NULL;
}


/* Set specific i-node as free 

	param id ... i-node id
*/ 
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


/*	Update size of all i-nodes on the path from the directory to root

	param dir ... the directory from which we move up in hierarchy
	param size ... adding size to originally size (can be negative if remove file)
*/
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


/*	Print info about file/directory

	param item ... file or directory
*/
void print_info(directory_item *item) {
	int i;
	int32_t number; // Data block number
	inode node = inodes[item->inode];
	
	printf("%s - %dB - i-node %d -", item->item_name, node.file_size, node.nodeid);
	printf(" Dir:");
	if (node.direct1 != FREE) {
		printf(" %d", node.direct1);
	}
	if (node.direct2 != FREE) {
		printf(" %d", node.direct2);
	}
	if (node.direct3 != FREE) {
		printf(" %d", node.direct3);
	}
	if (node.direct4 != FREE) {
		printf(" %d", node.direct4);
	}
	if (node.direct5 != FREE) {
		printf(" %d", node.direct5);
	}
	printf(" Indir:");
	if (node.indirect1 != FREE) {
		printf(" (%d)", node.indirect1);
		fseek(fs, sb->data_start_address + node.indirect1 * CLUSTER_SIZE, SEEK_SET);
		for (i = 0; i < MAX_NUMBERS_IN_BLOCK; i++) {
			fread(&number, sizeof(int32_t), 1, fs);
			if (number == 0) 
				break;
			printf(" %d", number);
		}
	}
	if (node.indirect2 != FREE) {
		printf(" (%d)", node.indirect2);
		fseek(fs, sb->data_start_address + node.indirect2 * CLUSTER_SIZE, SEEK_SET);
		for (i = 0; i < MAX_NUMBERS_IN_BLOCK; i++) {
			fread(&number, sizeof(int32_t), 1, fs);
			if (number == 0) 
				break;
			printf(" %d", number);
		}
	}
	printf("\n");
}


/* 	Print content of the file

	param item ... printing file
*/
void print_file(directory_item *item) {
	int i, tmp, block_count, rest, prev;
	int32_t *blocks;
	
	// Get data blocks of the file
	blocks = get_data_blocks(item->inode, &block_count, &rest); 

	prev = blocks[0];
	for (i = 0; i < block_count - 1; i++) {
		if (prev != blocks[i] - 1) {
			fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
		}
		fread(block_buffer, CLUSTER_SIZE, 1, fs);
		printf("%s", block_buffer);
		prev = blocks[i];
	}
	memset(block_buffer, 0, CLUSTER_SIZE);
	if (rest != 0)
		tmp = rest;
	else 
		tmp = CLUSTER_SIZE;
		
	fseek(fs, sb->data_start_address + blocks[block_count - 1] * CLUSTER_SIZE, SEEK_SET);
	fread(block_buffer, tmp, 1, fs);
	printf("%s", block_buffer);
	
	fflush(fs);
	free(blocks);
}


/* Printf the message of unformatted filesystem. */
void print_format_msg() {
	printf("The filesystem has to be formatted first.\nUsage: format [size]\n");
}

/*	Set i-node as file and initialize all data blocks

	param id ... i-node ID
	param size ... size of the file
	param block_count ... count of data blocks occupied by file
	param tmp_count ... block_count + blocks for indirect references
	param last_block_index ... address of the index to the last data block of the file
	param blocks ... data blocks
*/
void initialize_inode(int32_t id, int32_t size, int block_count, int tmp_count, int *last_block_index, int32_t *blocks) {
	int tmp;
	inode *node = &inodes[id];
	
	node->nodeid = id;
	node->isDirectory = 0;
	node->references = 1;
	node->file_size = size;
	node->direct1 = blocks[0];
	
	*last_block_index = 0;
	if (block_count > 1) {
		node->direct2 = blocks[1];
		*last_block_index = 1;
		
		if (block_count > 2) {
			node->direct3 = blocks[2];
			*last_block_index = 2;
			
			if (block_count > 3) {
				node->direct4 = blocks[3];
				*last_block_index = 3;
				
				if (block_count > 4) {
					node->direct5 = blocks[4];
					*last_block_index = 4;
					
					if (block_count > 5) {
						node->indirect1 = blocks[tmp_count - 1];
						
						
						if (block_count > 261) {
							node->indirect2 = blocks[tmp_count - 2];
							*last_block_index = tmp_count - 3;
							fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
							fwrite(&blocks[5], sizeof(int32_t), MAX_NUMBERS_IN_BLOCK, fs);
							
							tmp = block_count - 261;
							fseek(fs, sb->data_start_address + node->indirect2 * CLUSTER_SIZE, SEEK_SET);
							fwrite(&blocks[261], sizeof(int32_t), tmp, fs);
						}
						else  {
							*last_block_index = tmp_count - 2;
							tmp = block_count - 5;
							fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
							fwrite(&blocks[5], sizeof(int32_t), tmp, fs);
						}
					}
				}
			}
		}
	}
}


/*	Load filesystem from the file */
void load_fs() {
	directory *root;
	int i;
	
	if (!fs) {
		fs = fopen(fs_name, "rb+");
	}

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
	
//	printf("Size: %d\nCount of clusters: %d\nCount of i-nodes: %d\nCount of bitmap blocks: %d\nCount of i-node blocks: %d\nCount of data blocks: %d\nAddress of bitmap: %d\nAddress of i-nodes: %d\nAddress of data: %d\n", 
//	sb->disk_size, sb->cluster_count, sb->inode_count, sb->bitmap_cluster_count, sb->inode_cluster_count, sb->data_cluster_count, sb->bitmap_start_address, sb->inode_start_address, sb->data_start_address);
	
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


/*	Load all items of the directory from the file

	param dir ... directory
	param id ... directory id
*/
void load_directory(directory *dir, int id) {
	int i, j, block_count, rest;
	int inode_count = 64;	// Maximum count of i-nodes in one data block
	int32_t *blocks;		// Numbers of data blocks
	int32_t nodeid;			// Item id
	char name[12];			// Item name
	directory *newdir;
	directory_item *item, *temp;
	directory_item **psubdir = &(dir->subdir);	// Address of the last (free) subdirectory in the list of subdirectories
	directory_item **pfile = &(dir->file);		// Address of the last (free) file in the list of files
	
	// Get data blocks of this directory
	blocks = get_data_blocks(dir->current->inode, &block_count, NULL);
	
	for (i = 0; i < block_count; i++) {		// Iteration over data blocks
		fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
		for (j = 0; j < inode_count; j++) {	// Iteration over items in data block
			fread(&nodeid, sizeof(int32_t), 1, fs);		// Read inode id, if id < 1 -> invalid item and skip to the next item
			if (nodeid > 0) {
				fread(name, sizeof(name), 1, fs);
				item = create_directory_item(nodeid, name);
				if (inodes[nodeid].isDirectory) {	// If item is directory
					*psubdir = item;
					psubdir = &(item->next);
				}
				else {								// If item is file
					*pfile = item;
					pfile = &(item->next);
				}
			}
			else {
				fseek(fs, sizeof(name), SEEK_CUR);	// Skip the place for the name of the file/directory
			} 
		}	
	}
	free(blocks);

	// Recursive call this function on all loaded subdirectories
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


/*	Update bitmap in the file according to the file/directory

	param item ... file/directory
	param value ... 1 = use data blocks, 0 = free data blocks
	param data_blocks ... data blocks of the item or NULL (directory)
	param b_count ... count of blocks if not NULL
*/
void update_bitmap(directory_item *item, int8_t value, int32_t *data_blocks, int b_count) {
	int i, block_count, rest;
	int32_t *blocks; 
	
	if (!data_blocks) {
		blocks = get_data_blocks(item->inode, &block_count, NULL);
	}
	else {
		blocks = data_blocks;
		block_count = b_count;
	}
	for (i = 0; i < block_count; i++) {
		bitmap[blocks[i]] = value;
		fseek(fs, sb->bitmap_start_address + blocks[i], SEEK_SET);
		fwrite(&value, sizeof(int8_t), 1, fs);
	}

	// Indirect references blocks
	if (inodes[item->inode].indirect1 != FREE) {
		bitmap[inodes[item->inode].indirect1] = value;
		fseek(fs, sb->bitmap_start_address + inodes[item->inode].indirect1, SEEK_SET);
		fwrite(&value, sizeof(int8_t), 1, fs);
	}
	if (inodes[item->inode].indirect2 != FREE) {
			bitmap[inodes[item->inode].indirect2] = value;
			fseek(fs, sb->bitmap_start_address + inodes[item->inode].indirect2, SEEK_SET);
			fwrite(&value, sizeof(int8_t), 1, fs);
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


/*	Update directory - add/remove item from the file

	param dir ... directory
	param item ... item adding/removing to/from the directory
	param action ... 1 = add item, 0 = remove item
	return 0 = success, -1 = all data blocks of the directory are full or removing item was not found
*/
int update_directory(directory *dir, directory_item *item, int action) {
	int i, j, block_count, prev, item_count, found = 0;
	int32_t *blocks, *free_block;
	int name_length = 12;
	int zeros[4] = {0};  // buffer with zeros - for removing the item from the file
	int max_items_in_block = 64;
	int32_t nodeid;
	inode *dir_node;
	
	// Get data blocks
	blocks = get_data_blocks(dir->current->inode, &block_count, NULL);

	if (action == 1) {	// Store item (find free space)
		for (i = 0; i < block_count; i++) {
			fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
			for (j = 0; j < max_items_in_block; j++) {
				fread(&nodeid, sizeof(int32_t), 1, fs);
				if (nodeid == 0) {	// Free place found -> store item
					fseek(fs, -4, SEEK_CUR);
					fflush(fs);
					fwrite(&(item->inode), sizeof(int32_t), 1, fs);
					fwrite(item->item_name, sizeof(item->item_name), 1, fs);
					fflush(fs);
					free(blocks);
					return NO_ERROR;
				}
				else {
					fseek(fs, name_length, SEEK_CUR);	// Skip the space for name
				}
			}
		}
		
		// No free space was found in the current data blocks of the directory -> try to find another free data block
		free_block = find_free_data_blocks(1);	// Use direct reference
		if (!free_block) 
			return ERROR;
		
		dir_node = &(inodes[dir->current->inode]);
		
		if (dir_node->direct1 == FREE) {
			dir_node->direct1 = free_block[0];
		}
		else if (dir_node->direct2 == FREE) {
			dir_node->direct2 = free_block[0];
		}
		else if (dir_node->direct3 == FREE) {
			dir_node->direct3 = free_block[0];
		}
		else if (dir_node->direct4 == FREE) {
			dir_node->direct4 = free_block[0];
		}
		else if (dir_node->direct5 == FREE) {
			dir_node->direct5 = free_block[0];
		}
		else {
			free(free_block);
			free_block = find_free_data_blocks(2);	// Use indirect reference (need 2 free blocks)
			if (!free_block) 
				return ERROR;
				
			if (dir_node->indirect1 == FREE) {
				dir_node->indirect1 = free_block[1];
				fseek(fs, sb->data_start_address + free_block[1] * CLUSTER_SIZE, SEEK_SET);
				fwrite(&(free_block[0]), sizeof(int32_t), 1, fs);
			}
			else if (dir_node->indirect2 == FREE) {
				dir_node->indirect2 = free_block[1];
				fseek(fs, sb->data_start_address + free_block[1] * CLUSTER_SIZE, SEEK_SET);
				fwrite(&(free_block[0]), sizeof(int32_t), 1, fs);
			}
		}

		fseek(fs, sb->data_start_address + free_block[0] * CLUSTER_SIZE, SEEK_SET);
		fwrite(&(item->inode), sizeof(int32_t), 1, fs);
		fwrite(item->item_name, sizeof(item->item_name), 1, fs);
		
		fflush(fs);
		update_bitmap(dir->current, 1, NULL, 0);
		update_inode(dir->current->inode);
		free(free_block);
		free(blocks);
		return NO_ERROR;
	}
	else {	// Remove item (find the item with the specific id)
		prev = blocks[0];
		for (i = 0; i < block_count; i++) {
			if (prev != (blocks[i] - 1)) {
				fseek(fs, sb->data_start_address + blocks[i] * CLUSTER_SIZE, SEEK_SET);
			}

			item_count = 0;
			for (j = 0; j < max_items_in_block; j++) {
				fread(&nodeid, sizeof(int32_t), 1, fs);
				if (nodeid > 0)
					item_count++;
					
				if (!found) {
					if (nodeid == (item->inode)) {
						fseek(fs, -4, SEEK_CUR);
						fflush(fs);
						fwrite(&zeros, sizeof(zeros), 1, fs);
						fflush(fs);
						found = 1;
						if (item_count > 1)
							break;
					}
				}	
			}
			if (found) {	// If the only item in the data block was removing item -> free data block
				if (item_count == 1) {
					remove_reference(dir->current, i);
				}
				
				free(blocks);
				return NO_ERROR;
			}
			prev = blocks[i];
		}
	}
	return ERROR;
}


/*	Remove reference to the data block from i-node (except data block of direct1)
	(+from the file with indirect references)

	param item ... directory in which we remove data block
	param block_id ... number of the removing data block
*/
void remove_reference(directory_item *item, int32_t block_id) {
	int i, j, max_numbers = 256;
	int32_t number, count, found, zero = 0, blocks[2];
	inode *node = &inodes[item->inode];
	
	if (node->direct1 == block_id) {	// First direct block is not removing
		return;
	}
	else if (node->direct2 == block_id) {
		blocks[0] = node->direct2;
		node->direct2 = FREE;
	}
	else if (node->direct3 == block_id) {
		blocks[0] = node->direct3;
		node->direct3 = FREE;
	}
	else if (node->direct4 == block_id) {
		blocks[0] = node->direct4;
		node->direct4 = FREE;
	}
	else if (node->direct5 == block_id) {
		blocks[0] = node->direct5;
		node->direct5 = FREE;
	}
	else {
		for (i = 0; i < 2; i++) {
			if (i == 0)	// Go through indirect1
				fseek(fs, sb->data_start_address + node->indirect1 * CLUSTER_SIZE, SEEK_SET);
			else 		// Go through indirect2
				fseek(fs, sb->data_start_address + node->indirect2 * CLUSTER_SIZE, SEEK_SET);
			
			count = 0;
			found = 0;
			for (j = 0; j < max_numbers; j++) {
				fread(&number, sizeof(int32_t), 1, fs);
				if (number > 0) 
					count++;
				if (!found) {
					if (number == block_id) {
						found = 1;
						blocks[0] = number;
						fseek(fs, -4, SEEK_CUR);
						fflush(fs);
						fwrite(&zero, sizeof(int32_t), 1, fs);
						fflush(fs);
						if (count > 1)
							break;
					}
				}
			}
			
			if (found) {
				if (count == 1) {
					if (i == 0) {
						blocks[1] = node->indirect1;
						node->indirect1 = FREE;
					}
					else {
						blocks[1] = node->indirect2;
						node->indirect2 = FREE;
					}
				}
				break;
			}
		}
	}
	
	update_bitmap(item, 0, blocks, 1);
	update_inode(item->inode);
}

