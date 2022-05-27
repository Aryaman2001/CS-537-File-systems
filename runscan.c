#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "ext2_fs.h"
#include "read_ext2.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
	if (argc != 3) {
		printf("expected usage: ./runscan inputfile outputfile\n");
		exit(0);
	}
	
	// Throw error if directory already exists, otherwise create output dir
	if (opendir(argv[2]) != NULL) {
		printf("Error occurred, directory already exists\n");
		exit(0);
	}
	
	int created = mkdir(argv[2], S_IRWXU);
	if (created == -1) {
		printf("Error occurred while creating directory\n");
		exit(0);
	}

	int fd;
	fd = open(argv[1], O_RDONLY);    /* open disk image */

	ext2_read_init(fd);

	struct ext2_super_block super;
	struct ext2_group_desc group;

	// example read first the super-block and group-descriptor
	read_super_block(fd, 0, &super);
	read_group_desc(fd, 0, &group);
	// printf("There are %u inodes in an inode table block and %u blocks in the idnode table\n", inodes_per_block, itable_blocks);
	
	// traversing through each block group
	// int num_block_groups = (super.s_blocks_count + super.s_blocks_per_group - 1) / super.s_blocks_per_group;
	// while (i < num_block_groups) {
	// 	read_super_block(fd, i, &super);
	//	read_group_desc(fd, i, &group);
	// } 
	
	
	int block_size = 1024 << super.s_log_block_size;	// size of a block
	int inodes_per_block = block_size / sizeof(struct ext2_inode);	// number of inodes per block
	int total_blocks = super.s_inodes_per_group / inodes_per_block;	// size in blocks of the inode table

	off_t start_inode_table = locate_inode_table(0, &group);	// start of inode table for this group
	int inode_no = 0;	// keeps track of inode number

	int jpg_inodes[1024];
	int jpg_idx = 0;

	// iterates over the blocks containing inodes
	for (int block_count = 0; block_count < total_blocks; block_count++) {

		//iterates all the inodes in a block
		for (int i = 0; i < inodes_per_block; i++) {
			// printf("inode number %u: \n", inode_no);
			struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
			read_inode(fd, start_inode_table, inode_no, inode);
			
			// Skipping if its not a regular file.
			
			if (!S_ISREG(inode->i_mode)) {
				inode_no++;
				free(inode);
				continue;
			}

			// Reading first data block to check for jpg
			char buffer[1024];
			lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET); // block_count * 1024 + 
			read(fd, buffer, 1024);
		
			if (buffer[0] == (char)0xff &&
				buffer[1] == (char)0xd8 &&
				buffer[2] == (char)0xff &&
				(buffer[3] == (char)0xe0 ||
				buffer[3] == (char)0xe1 ||
				buffer[3] == (char)0xe8)) {
					// adding jpg inode no to array for dir traversal
					jpg_inodes[jpg_idx] = inode_no;
					jpg_idx++;
			} else {
				inode_no++;
				free(inode);
				continue;
			}
				

			int file_size = inode->i_size;
			// printf("file size: %d\n", file_size);
			
			// Creating and opening file
			FILE *output_file;
			
			char file_name[256] = "";
			strcat(file_name, argv[2]);
			strcat(file_name, "/file-");
			char number[sizeof(int) * 4 + 1];
			sprintf(number, "%d", inode_no);
			strcat(file_name, number);
			strcat(file_name, ".jpg");
			output_file = fopen(file_name, "a+");
			if (output_file == NULL) {
				printf("Error occurred while creating file");
				exit(0);
			}
			
			// iterates over data block pointers of the inode
			for(unsigned int i=0; i<EXT2_N_BLOCKS; i++) {    
				// reading in data block
				// printf("Block pointer: %d\n\n", inode->i_block[i]);
				
				uint add_buffer[256];
				if (i >= EXT2_NDIR_BLOCKS) {
					lseek(fd, BLOCK_OFFSET(inode->i_block[i]), SEEK_SET); // block_count * 1024 + 
					read(fd, add_buffer, 1024);
				} else {
					lseek(fd, BLOCK_OFFSET(inode->i_block[i]), SEEK_SET); // block_count * 1024 + 
					read(fd, buffer, 1024);
				}

				// skipping if file already read
				if (file_size <= 0) {
					break;
				}
				int output_fd = open(file_name, O_APPEND | O_RDWR);

				// writing to file based on direct/indirect
				if (i < EXT2_NDIR_BLOCKS) {                   
					if (file_size > 1024) {
						write(output_fd, buffer, 1024);
						file_size -= 1024;
					} else {
						write(output_fd, buffer, file_size);
						file_size = 0;
						break;
					}
				}  else if (i == EXT2_IND_BLOCK) {
					char indir_buffer[1024];
					// add_buffer has pointers to blocks
					// now seek and read these pointers
					for (int i = 0; i < 256; i++) {
						lseek(fd, BLOCK_OFFSET(add_buffer[i]), SEEK_SET); // block_count * 1024 + 
						read(fd, indir_buffer, 1024);
						// write to file!
						// printf("FILE SIZE: %d\n", file_size);

						if (file_size > 1024) {
							write(output_fd, indir_buffer, 1024);
							file_size -= 1024;
						} else {
							write(output_fd, indir_buffer, file_size);
							file_size = 0;
							break;
						}
   					}
				}	else if (i == EXT2_DIND_BLOCK) { 
						uint indir_add_buffer[256];
						// add_buffer has pointers to pointers to blocks
						// now seek and load these pointers
						for (int j = 0; j < 256; j++) {
							lseek(fd, BLOCK_OFFSET(add_buffer[j]), SEEK_SET); // block_count * 1024 + 
							read(fd, indir_add_buffer, 1024);

							// indir_add_buffer has pointers to blocks
							// now seek and load these pointers in indir_buffer
							char indir_buffer[1024];
							for (int k = 0; k < 256; k++) {
								lseek(fd, BLOCK_OFFSET((uint)indir_add_buffer[k]), SEEK_SET); // block_count * 1024 + 
								read(fd, indir_buffer, 1024);

								// write to file!							
								// printf("FILE SIZE: %d\n", file_size);

								if (file_size > 1024) {
									write(output_fd, indir_buffer, 1024);
									file_size -= 1024;
								} else {
									write(output_fd, indir_buffer, file_size);
									file_size = 0;
									break;
								}
							}
				 		} 
				}	else if (i == EXT2_TIND_BLOCK) {
						uint indir_indir_add_buffer[256];
						// add_buffer has pointers to pointers to pointers to blocks
						// now seek and load these pointers
						for (int j = 0; j < 256; j++) {
							lseek(fd, BLOCK_OFFSET(add_buffer[j]), SEEK_SET); // block_count * 1024 + 
							read(fd, indir_indir_add_buffer, 1024);

							// indir_indir_add_buffer has pointers to pointers to blocks
							// now seek and load these pointers in indir_add_buffer
							uint indir_add_buffer[256];
							for (int k = 0; k < 256; k++) {
								lseek(fd, BLOCK_OFFSET((uint)indir_indir_add_buffer[k]), SEEK_SET); // block_count * 1024 + 
								read(fd, indir_add_buffer, 1024);

								// indir_add_buffer has pointers to blocks
								// now seek and load these pointers in indir_buffer
								char indir_buffer[1024];
								for (int k = 0; k < 256; k++) {
									lseek(fd, BLOCK_OFFSET((uint)indir_add_buffer[k]), SEEK_SET); // block_count * 1024 + 
									read(fd, indir_buffer, 1024);

									// write to file!							
									// printf("FILE SIZE: %d\n", file_size);

									if (file_size > 1024) {
										write(output_fd, indir_buffer, 1024);
										file_size -= 1024;
									} else {
										write(output_fd, indir_buffer, file_size);
										file_size = 0;
										break;
									}
								}
							}
				 		}
				}
				close(output_fd);          
			}
			fclose(output_file);					
			free(inode);
			inode_no++;
        }
	}
	close(fd);
	// directory traversal
	int new_fd = open(argv[1], O_RDONLY);    /* open disk image */
	ext2_read_init(new_fd);

	struct ext2_super_block new_super;
	struct ext2_group_desc new_group;

	read_super_block(new_fd, 0, &new_super);
	read_group_desc(new_fd, 0, &new_group);

	block_size = 1024 << new_super.s_log_block_size;	// size of a block
	inodes_per_block = block_size / sizeof(struct ext2_inode);	// number of inodes per block
	total_blocks = new_super.s_inodes_per_group / inodes_per_block;	// size in blocks of the inode table

	off_t new_start_inode_table = locate_inode_table(0, &new_group);	// start of inode table for this group
	int new_inode_no = 0;	// keeps track of inode number

	for (int block_num = 0; block_num < total_blocks; block_num++) {
		for (int i = 0; i < inodes_per_block; i++) {
			struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
			read_inode(new_fd, new_start_inode_table, new_inode_no, inode);
			if (!S_ISDIR(inode->i_mode)) {
                free(inode);
				new_inode_no++;
                continue;
        	}
			char dir_buffer[1024];
			lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET); // block_count * 1024 + 
			read(fd, dir_buffer, 1024);
			int start = 0;
			while (start < 1024) {
				struct ext2_dir_entry *dentry = (struct ext2_dir_entry*) & ( dir_buffer[start] );
				if (dentry->inode == 0 || dentry->rec_len == 0 || dentry->name_len == 0) {
					break;
				}
				
				int is_jpg = 0;
				for (int j = 0; j < jpg_idx; j++) {
					if ((int) dentry->inode == jpg_inodes[j]) {
						is_jpg = 1;
						break;
					}
				}
				if (is_jpg == 1) {
					//char new_dentry_name[1024];
					FILE* old_file;
					FILE* second_file;
					char old_file_name[256] = "";
					char second_file_name[256] = "";
					strcat(old_file_name, argv[2]);
					strcat(old_file_name, "/file-");
					char file_number[sizeof(int) * 4 + 1];
					sprintf(file_number, "%d", dentry->inode);
					strcat(old_file_name, file_number);
					strcat(old_file_name, ".jpg");
					old_file = fopen(old_file_name, "a+");
					fclose(old_file);
					strcat(second_file_name, argv[2]);
					strcat(second_file_name, "/");
					char name [EXT2_NAME_LEN];
					int name_len = dentry->name_len & 0xFF; // convert 2 bytes to 4 bytes properly
					strncpy(name, dentry->name, name_len);
					name[name_len] = '\0';
					strcat(second_file_name, name);
					printf("dentry name: %s\n", name);
					second_file = fopen(second_file_name, "a+");
					fclose(second_file);

					int old_fd = open(old_file_name, O_APPEND | O_RDWR);
					int second_fd = open(second_file_name, O_APPEND | O_RDWR);
					char *old_buffer = malloc(sizeof(char) * 100000000);
					int old_size = read(old_fd, old_buffer, 100000000);
					write(second_fd, old_buffer, old_size);
					close(old_fd);
					close(second_fd);
				}
				int name_len_rounded = dentry->name_len;
				if (dentry->name_len % 4 != 0) {
					int extra = 4 - (dentry->name_len % 4);
					name_len_rounded = dentry->name_len + extra;
				}
				start = start + 8 + name_len_rounded;
			}
            free(inode);
			new_inode_no++;
		}
	}
	close(new_fd);
}