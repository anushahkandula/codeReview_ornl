#include "lib.h"
#include "file_sys.h"

//size of 4kB in bytes
#define _4kB 4096

//address for loading of programs
#define PROGRAMADDR				 0x08048000

/* read_dentry_by_name
 * Description: populates the provided dentry struct with information corrosponding to the
                provided file name.
 * Input: fname: the file name as a string, must be less than 32 characters
 * Returns: 0 on success, -1 on fail
 * Output: Nothing
 * Side effects: dentry is populated with file data.
 */
int32_t read_dentry_by_name(const uint8_t* fname, dentry_t* dentry)
{
int counter;
boot_block_t* block_ = (boot_block_t*)boot_block_addr;
int index = -1; // -1 is error value used to check whether or not file was found
int s_length = strlen((int8_t*)fname);
//check for invalid name size
 if (s_length > SIZE_NAME)
 {
     return -1;
 }
  //checks all entries for matching fname and updates index if found
  for(counter = 0; counter < block_->num_dentries; counter++)
  {
    if(strncmp((int8_t*)fname, (int8_t*)(block_->dentries[counter].filename), SIZE_NAME) == 0)//0 corresponds with strings being the same
    {
        index = counter;
        break;
    }
  }
  //if we have a valid entry, look it up by index
  if(index != -1) //-1 corresponds with file not being found
  {
    return read_dentry_by_index(index, dentry);
  }
  else
  {
    return index;
  }
  //returns -1 if it fails to find matching file name
}

/* read_dentry_by_index
 * Description: populates the provided dentry struct with information corrosponding to the
                provided file index
 * Input: index: dentry index corrosponding to the desired file
 * Returns: 0 on success, -1 on fail
 * Output: Nothing
 * Side effects: dentry is populated with file data.
 */
int32_t read_dentry_by_index(uint32_t index, dentry_t* dentry)
{
    boot_block_t* block_ = (boot_block_t*)boot_block_addr;

    if(index >= 0 && index < block_->inode_count) //index being less than 0 makes no sense
    {
      dentry->filetype = block_->dentries[index].filetype; //copy index into dentry
      strcpy((int8_t*)dentry->filename, (int8_t*)block_->dentries[index].filename); //copy filename into dentry
      dentry->inodenum = block_->dentries[index].inodenum; //copy inode into dentry
      return 0; //return success
    }
    else
    {
      return -1; //otherwise return negative one if out of bounds
    }
}

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
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length)
{
  boot_block_t* block_ = (boot_block_t*)boot_block_addr;
  //calculate inode location which is offset by 1 because of the boot block
  inode_t* inod = (inode_t*)(boot_block_addr + _4kB * (inode + 1));
  //claculate the start of the data blocks after the boot block(+1) and all the inodes
  uint32_t data_start = boot_block_addr + _4kB * (block_->inode_count + 1);
  int32_t bytes_read = 0;//initialize number of bytes read to 0
  int32_t current_data_block_num;

  //check for invalid inodes
  if (inode < 0 || inode >= block_->inode_count) {//inode number being less than 0 makes no sense
    return -1; //return an error
  }
  //go untill all bytes read of EOF
  while((bytes_read < length) && (offset + bytes_read < inod->length)) {
    current_data_block_num = (offset+bytes_read)/_4kB;
    //check for data block validity (data block number being less than 0 makes no sense)
    if (current_data_block_num < 0 || current_data_block_num >= block_->data_count) {
      return -1; //return and error
    }
    //read from the data block into the buffer
    buf[bytes_read] = ((uint8_t*)(data_start + _4kB * inod->data_block_num[current_data_block_num]))[(offset+bytes_read)%_4kB];
    bytes_read++;
  }
  return bytes_read;
}


/* file_open
 * Description: initializes a file from the system so it can be read
 * Input: filename - the file name as a string
 * Returns: 0 on success
 * Output: Nothing
 * Side effects: none
 */
int32_t file_open (const uint8_t* filename)
{
  return 0;//return success
}

/* file_close
 * Description: does nothing for now
 * Input: file_d - file descriptor number
 * Returns: 0 on success, -1 on fail
 * Output: Nothing
 * Side effects: none
 */
int32_t file_close (int32_t file_d)
{
  // if (file_d == 0 || file_d == 1)
  //     return -1;
  return 0; //return success
}

/* file_write
 * Description: does nothing - its a read only file system
 * Input: file_d - file descriptor number
          buf - buffer to write from
          nbytes- number of bytes to read
 * Returns: -1 always for fail
 * Output: Nothing
 * Side effects: none
 */
int32_t file_write (int32_t file_d, const void* buf, int32_t nbytes)
{
    return -1;
}

/* file_read
 * Description: reads data from a file to a buffer
 * Input: file_d - file descriptor number
          buf - buffer to read to
          nbytes- number of bytes to read
 * Returns: number of bytes read on success, -1 always for fail
 * Output: Nothing
 * Side effects: none
 */
int32_t file_read (int32_t file_d, void* buf, int32_t nbytes)
{
    pcb_t* curPCB = get_pcb();                                                  //get pcb of current process
    buf = (uint8_t*) buf;                                                       //cast buffer into string
    int32_t count_bytes;                                                        //will hold number of bytes read
    //check for invalid pointer and number of bytes
    if (buf == NULL || nbytes < 0)
        return -1;
    //call read data
    count_bytes = read_data((curPCB->file_descriptor[file_d]).inode_num, (curPCB->file_descriptor[file_d]).file_position, buf, nbytes);
    //move the file cursor
    (curPCB->file_descriptor[file_d]).file_position += count_bytes;
    return count_bytes;//return number of bytes read
}

/* get_file_type
 * Description: helper function that gets the file type for a file name
 * Input: fname - string that is the name of the file
 * Returns: file type number
 * Output: Nothing
 * Side effects: none
 */
int32_t get_file_type(uint8_t* fname) {
  dentry_t d;
  //fetch dentry info
  read_dentry_by_name(fname, &d);
  //return filetype
  return d.filetype;
}

/* program_load
 * Description: loads a executable file to appropriate virtual address offset
 * Input: name of file to load (behavior undefined if file does not exist or is not executable)
 * Returns: Nothing
 * Output: Nothing
 * Side effects: program is loaded
 */
void program_load(const uint8_t* fname)
{
  uint8_t* curAddress = (uint8_t*)PROGRAMADDR;                                  //address we are currently writing to
  uint32_t bytesToRead =  200;                                                  //number of bytes to read at a time(value is arbitrary)
  uint32_t bytesRead;                                                           //used to hold number of bytes read;
  uint32_t offset = 0;                                                          //current offset into file(start at beginning of file)
  dentry_t curDentry;                                                           //will hold dentry of current file


  //load the appropriate dentry
  read_dentry_by_name(fname, &curDentry);

  //read first 200 bytes from file directly into memory
  bytesRead = read_data(curDentry.inodenum, offset, curAddress, bytesToRead);
  //continue reading from file untill end of file
  while(bytesRead == bytesToRead)
  {
      curAddress += bytesRead; //increment location to copy to
      offset += bytesRead; //increment offset in file
      bytesRead = read_data(curDentry.inodenum, offset, curAddress, bytesToRead);//copy data
  }
}
