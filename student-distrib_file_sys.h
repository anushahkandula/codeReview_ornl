#ifndef _FILE_SYS_H
#define _FILE_SYS_H

//size of a data block
#define BLOCK_SIZE 4096

//size of a filename
#define SIZE_NAME 32

//number of reserved bytes in dentry. In order to make the entire dentry 64 bytes large(rest of elements in struct have 40 bytes, and 64-40=24)
#define DENTRY_RESERVED   24

//number of reserved bytes in boot block. In order to make the first entry of the boot block 64 bytes large(elements of struct above this one have 12 bytes, and 64-12=52)
#define BOOT_RESERVED     52

//maximum number of directory entries that will fit into in boot block
#define MAX_ENTRIES 63

//number of data block numbers that will fit into an inode -- number of 4 byte integers that will fit in 4kB(4kB/4-1= 1023). -1 accounts for the length field in the inode
#define NUM_DATA_BLOCK_NUMS   1023

#include "lib.h"
#include "types.h"
#include "sys_calls.h"

//location of the boot block in memory
uint32_t boot_block_addr;

//defines the structure of a dentry
typedef struct {
    uint8_t filename[SIZE_NAME];
    int32_t filetype;
    int32_t inodenum;
    int8_t reserved[DENTRY_RESERVED];
} dentry_t;

//defines the structure of the boot block
typedef struct {
    int32_t num_dentries;
    int32_t inode_count;
    int32_t data_count;
    int8_t reserved[BOOT_RESERVED];
    dentry_t dentries[MAX_ENTRIES];
} boot_block_t;

typedef struct {
    int32_t length;
    int32_t data_block_num[NUM_DATA_BLOCK_NUMS];
} inode_t;

/* file_open
 * Description: initializes a file from the system so it can be read
 * Input: filename - the file name as a string
 * Returns: 0 on success, -1 on fail
 * Output: Nothing
 * Side effects: none
 */
int32_t file_open (const uint8_t* filename);

/* file_read
 * Description: reads data from a file to a buffer
 * Input: file_d - file descriptor number
          buf - buffer to read to
          nbytes- number of bytes to read
 * Returns: number of bytes read on success, -1 always for fail
 * Output: Nothing
 * Side effects: none
 */
int32_t file_read (int32_t file_d, void* buf, int32_t nbytes);

/* file_write
 * Description: does nothing - its a read only file system
 * Input: file_d - file descriptor number
          buf - buffer to read to
          nbytes- number of bytes to read
 * Returns: -1 always for fail
 * Output: Nothing
 * Side effects: none
 */
int32_t file_write (int32_t file_d, const void* buf, int32_t nbytes);

/* file_close
 * Description: does nothing for now
 * Input: file_d - file descriptor number
 * Returns: 0 on success, -1 on fail
 * Output: Nothing
 * Side effects: none
 */
int32_t file_close (int32_t file_d);

/* read_dentry_by_name
 * Description: populates the provided dentry struct with information corrosponding to the
                provided file name.
 * Input: fname: the file name as a string, must be less than 32 characters
 * Returns: 0 on success, -1 on fail
 * Output: Nothing
 * Side effects: dentry is populated with file data.
 */
int32_t read_dentry_by_name(const uint8_t* fname, dentry_t* dentry);

/* read_dentry_by_index
 * Description: populates the provided dentry struct with information corrosponding to the
                provided file index
 * Input: index: dentry index corrosponding to the desired file
 * Returns: 0 on success, -1 on fail
 * Output: Nothing
 * Side effects: dentry is populated with file data.
 */
int32_t read_dentry_by_index(uint32_t index, dentry_t* dentry);


/* read_data
 * Description: Reads data from a file to a buffer.
 * Input: inode - the index of the inode corrosponding to the file being read
          offset - offset in bytes that we are starting from the beginning
          buf - char buffer that is being read to
          lenght - max number of bytes to read
 * Returns: number of bytes read on success, -1 on fail
 * Output: Nothing
 * Side effects: none
 */
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length);

/* get_file_type
 * Description: helper function that gets the file type for a file name
 * Input: fname - string that is the name of the file
 * Returns: file type number
 * Output: Nothing
 * Side effects: none
 */
int32_t get_file_type(uint8_t* fname);

//loads a executable file to appropriate virtual address offset
void program_load(const uint8_t* fname);

#endif
