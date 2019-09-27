#include "RTC.h"
#include "directories.h"
#include "lib.h"
#include "paging.h"
#include "x86_desc.h"
#include "sys_calls.h"

//file descriptor number corresponding with stdout
#define STDOUT 1

//holds whether or not process id is available (implicity counts number of processes)
//We initially reserve pids 0-2 so that we can make suere that they get assigned to the base shells
int32_t pidAvailable[MAX_PROCESSES] = {NOT_AVAILABLE, NOT_AVAILABLE, NOT_AVAILABLE, AVAILABLE, AVAILABLE, AVAILABLE};

//holds whether or not the process is currently active (pid = index)
int32_t activeProcess[MAX_PROCESSES] = {INACTIVE, INACTIVE, INACTIVE, INACTIVE, INACTIVE, INACTIVE};

//holds whether or not the process has been run in this cycle
int32_t beenRun[MAX_PROCESSES] = {NOT_RUN, NOT_RUN, NOT_RUN, NOT_RUN, NOT_RUN, NOT_RUN};

//holds currently running process (initialized to 10 to indicate no process)
int32_t currentPid = NO_PROCESS;

//tables of file operations
int32_t (*terminal_operations[])() = {terminal_open, terminal_read, terminal_write, terminal_close};
int32_t (*rtc_operations[])() = {rtcOpen, rtcRead, rtcWrite, rtcClose};
int32_t (*file_operations[])() = {file_open, file_read, file_write, file_close};
int32_t (*directory_operations[])() = {directory_open, directory_read, directory_write, directory_close};

/* halt
 * Description: handler for halt system call (halts user level program)
 * Inputs: status(return value for execute)
 * Outputs: Nothing
 * Returns: Does not return(return call in this function actually returns from appropriate execute call)
 * Side Effects: stops user-level program
 */
int32_t halt (uint32_t status){
	pcb_t* curPCB;																																//will hold current process's pcb
	int i;																																		//loop counter

	//get current process's pcb
	curPCB = get_pcb();

	//if process has no parent(shell) free current pid, remove pid from terminal struct, reset current pid variable, and then rerun program
	if((curPCB->parentPid == NO_PROCESS))
	{
		pidAvailable[currentPid] = AVAILABLE;
		//find pid in PID array
		for(i = 0; i < ((curPCB->terminal)->num_processes); i++)
		{
			//if pid is found, remove it by replacing it with the last element in array and decrement num_processes field
			if(((curPCB->terminal)->PID)[i] == currentPid)
			{
				((curPCB->terminal)->PID)[i] = ((curPCB->terminal)->PID)[--((curPCB->terminal)->num_processes)];
				break;
			}
		}
		currentPid = NO_PROCESS;
		terminal_write(STDOUT, "ERROR, RESTARTING SHELL\n", strlen("ERROR, RESTARTING SHELL\n"));
		execute((uint8_t*)"shell");
	}

	//close all of the file descriptors that are open
	for(i = 0; i < FILE_DESC_NUM; i++)
	{
		//check if file descriptor is open
		if((curPCB->file_descriptor[i].flags & (1 << PRESENT)) != 0)
		{
			//call close function for file descriptor
			(((curPCB->file_descriptor)[i].filename_operations[CLOSE])(i));
			//set all flags to 0 (rest of flags don't matter since closed)
			curPCB->file_descriptor[i].flags = 0;
		}
	}

	//update terminal struct corresponding to process

	//find pid in PID array
	for(i = 0; i < ((curPCB->terminal)->num_processes); i++)
	{
		//if pid is found, remove it by replacing it with the last element in array and decrement num_processes field
		if(((curPCB->terminal)->PID)[i] == currentPid)
		{
			((curPCB->terminal)->PID)[i] = ((curPCB->terminal)->PID)[--((curPCB->terminal)->num_processes)];
			break;
		}
	}

	//change the active process back to the parent process
	(curPCB->terminal)->active_process_pid = curPCB->parentPid;

	//mark current pid as available and inactive and set current pid to parent's pid
	pidAvailable[currentPid] = AVAILABLE;
	activeProcess[currentPid] = INACTIVE;
	currentPid = curPCB->parentPid;

	//set parent process back to active and schedule it to be run
	activeProcess[curPCB->parentPid] = ACTIVE;
	beenRun[curPCB->parentPid] = NOT_RUN;

	//set current page to parent's page
	Page_Directory[STARTMB/PDOFFSETMB] = PHYSSTARTADDR + currentPid * PHYSADDROFFSET;
	//set flags to appropriate values
	Page_Directory[STARTMB/PDOFFSETMB] |= ((1 << PRESENT) | (1 << RW) | (1 << US) | (1 << PCD) | (1 << PS));

	//reload cr3 to flush the tlb
  asm volatile ("movl %%eax, %%CR3"
                :
                : "a" (Page_Directory)
                );

	//set tss esp0 and ss0 to appropriate values (parent kernel stack)
	tss.esp0 = KERNEL_ADDR_END - (STACK_SIZE_KERNEL *currentPid)-LONG; // take stack size end of kernel
	tss.ss0 = KERNEL_DS;

	//set current stack pointer and base pointer to parent's stack and base pointer
	asm volatile(
				"movl %0, %%esp			\n"
				"movl %1, %%ebp			\n"
				/*call return(technically returning from execute b/c of kernel stack that is set but code is placed here)*/
				"movl %2, %%eax			\n"
				"leave							\n"
				"ret								\n"
				:
				: "r"(curPCB->parentESP), "r"(curPCB->parentEBP), "r"(status)
	);
	//will never run this return statement, but compiler will complain otherwise (0 is arbitrary)
	return 0;
}

/* execute
 * Description: handler for execute system call (executes user level program)
 * Inputs: command - program to call plus arguments for program
 * Outputs: Nothing
 * Returns: Value from user level program or negative 1 if cannot execute or 256 if program dies by exception
 * Side Effects: Calls user level program
 */
int32_t execute (const uint8_t* command)
{
	const uint8_t * curChar;													  													//points to current character in command array(used to parse args)
	uint8_t filename[SIZE_NAME];																									//will hold the filename of the executable
	uint32_t charsCopied;																													//counter used when copying data from command
	uint32_t counter;																															//used for assigning process id
	pcb_t* curPCB;																																//pcb for the process we are initializing
	uint32_t curPid;																															//pid for current process
	dentry_t fileDentry;																													//used to read the file
	uint8_t buf[50];																													  	//used to read first 27 bytes of file to check if executable and get start offset (50 is arbritrary value greater than 27)
	int i;//loop counter

	/*Step 1: assign pid and check if another process can be run. In other words
		we haven't maxed out on number of processes we can run.*/

	//assign pid for process(same as process number except zero indexed)
	counter = 0;
	while(counter < MAX_PROCESSES)
	{
		//if pid is available, assign it to current process and then break from loop
		if(pidAvailable[counter] == 0)
		{
			curPid = counter;
			pidAvailable[counter] = NOT_AVAILABLE;
			break;
		}
		counter++;
	}
	//if no available pid was found, number of processes has been maxed out, so return an error
	if(counter == MAX_PROCESSES)
	{
		//set parent process back to active, then return an error
		activeProcess[currentPid] = ACTIVE;
		return -1;
	}

	/*Step 2: initializes the pcb and gets the pcb so that we can update the fields
		of it. Then, we set the pid field of the pcb.*/

	//initializes pcb for process
	setup_pcb(curPid);

	//gets the pcb for process
	curPCB = (pcb_t*)(KERNEL_ADDR_END - (STACK_SIZE_KERNEL * (curPid + 1))); //get pointer to top of appropriate kernel stack/bottom of kernel stack above the given process's kernel stack(where pcb will be located), +1 corresponds with the fact that we want the top of the appropriate kernel stack

	//set pcb's pid
	curPCB->pid = curPid;

	/*Parse the given string and check for null input. Also save the arguments in the pcb here.*/

	//if input is null, return an error
	if(command == NULL)
	{
		//update terminal struct corresponding to process

		//change the active process back to the parent process
		(curPCB->terminal)->active_process_pid = curPCB->parentPid;

		//find pid in PID array
		for(i = 0; i < ((curPCB->terminal)->num_processes); i++)
		{
			//if pid is found, remove it by replacing it with the last element in array and decrement num_processes field
			if(((curPCB->terminal)->PID)[i] == currentPid)
			{
				((curPCB->terminal)->PID)[i] = ((curPCB->terminal)->PID)[--((curPCB->terminal)->num_processes)];
				break;
			}
		}
		//set parent process back to active, then return an error
		activeProcess[currentPid] = ACTIVE;
		pidAvailable[curPid] = AVAILABLE;
		return -1;
	}

	charsCopied = 0;//reset counter to 0
	curChar = command;//set current character to first character in given string
	//copy all characters before space or null terminating character or newline character into filename or 32 characters, whichever comes first
	memset(filename, 0, SIZE_NAME);
	while((charsCopied < SIZE_NAME) && (*curChar != ' ') && (*curChar != '\0') && (*curChar != '\n'))
	{
		filename[charsCopied] = *curChar;
		curChar++;
		charsCopied++;
	}
	//cut off rest of executable name
	while((*curChar != ' ') && (*curChar != '\0') && (*curChar != '\n'))
	{
		curChar++;
	}
	//if executable name is shorter than 32 characters, place null character at end
	if(charsCopied < SIZE_NAME)
	{
		filename[charsCopied] = '\0';
	}

	//if current character is null character or newline character there are no args, so place empty string in args
	if((*curChar == '\0') || (*curChar == '\n'))
	{
		curPCB->args[0] = '\0';
	}
	else
	{
		//remove leading spaces for args
		while(*curChar == ' ')
		{
			curChar++;
		}

		//otherwise, copy args into arguments array
		charsCopied = 0;//reset counter
		while((*curChar != '\0') && (*curChar != '\n'))
		{
			curPCB->args[charsCopied] = *curChar;
			curChar++;
			charsCopied++;
		}
		//place null character
		curPCB->args[charsCopied] = '\0';
	}

	//save the filename in the PCB
	memcpy(curPCB->name, filename, SIZE_NAME);

	/*Step 3: Get the file if it exists. If it does not exist, return an error*/

	//get the corresponding file, if it exists
	if(read_dentry_by_name(filename, &fileDentry) != 0)
	{
		//update terminal struct corresponding to process

		//change the active process back to the parent process
		(curPCB->terminal)->active_process_pid = curPCB->parentPid;

		//find pid in PID array
		for(i = 0; i < ((curPCB->terminal)->num_processes); i++)
		{
			//if pid is found, remove it by replacing it with the last element in array and decrement num_processes field
			if(((curPCB->terminal)->PID)[i] == currentPid)
			{
				((curPCB->terminal)->PID)[i] = ((curPCB->terminal)->PID)[--((curPCB->terminal)->num_processes)];
				break;
			}
		}
		//mark current pid as available
		pidAvailable[curPid] = AVAILABLE;
		//set parent process back to active, then return an error
		activeProcess[currentPid] = ACTIVE;
		//if file does not exist, return -1
		return -1;
	}

	//read first 28 bytes of file (start of file == 0 offset)
	if(read_data(fileDentry.inodenum, 0, buf, 28) < 28)
	{
		//update terminal struct corresponding to process

		//change the active process back to the parent process
		(curPCB->terminal)->active_process_pid = curPCB->parentPid;

		//find pid in PID array
		for(i = 0; i < ((curPCB->terminal)->num_processes); i++)
		{
			//if pid is found, remove it by replacing it with the last element in array and decrement num_processes field
			if(((curPCB->terminal)->PID)[i] == currentPid)
			{
				((curPCB->terminal)->PID)[i] = ((curPCB->terminal)->PID)[--((curPCB->terminal)->num_processes)];
				break;
			}
		}
		//set current pid as available
		pidAvailable[curPid] = AVAILABLE;
		//set parent process back to active, then return an error
		activeProcess[currentPid] = ACTIVE;
		//if not enough bytes were read, file cannot be an executable, so return an error
		return -1;
	}

	/*Step 4: Make sure that the file is an executable and save the start address
		saved in it. If file is not an executable return an error.*/

	//if first 4 bytes are not the correct magic numbers, return an error
	if((buf[0] != MAGIC1) || (buf[1] != MAGIC2) || (buf[2] != MAGIC3) || (buf[3] != MAGIC4))
	{
		//update terminal struct corresponding to process

		//change the active process back to the parent process
		(curPCB->terminal)->active_process_pid = curPCB->parentPid;

		//find pid in PID array
		for(i = 0; i < ((curPCB->terminal)->num_processes); i++)
		{
			//if pid is found, remove it by replacing it with the last element in array and decrement num_processes field
			if(((curPCB->terminal)->PID)[i] == currentPid)
			{
				((curPCB->terminal)->PID)[i] = ((curPCB->terminal)->PID)[--((curPCB->terminal)->num_processes)];
				break;
			}
		}
		//set pid back to available
		pidAvailable[curPid] = AVAILABLE;
		//set parent process back to active, then return an error
		activeProcess[currentPid] = ACTIVE;
		return -1;
	}

	//compute appropriate starting offset using bytes 24-27 of file
	curPCB->startAddress = (uint32_t*)((buf[24] << BYTE4) | (buf[25] << BYTE3) | (buf[26] << BYTE2) | (buf[27] << BYTE1));

	/*Step 5: Allocate a 4 MB page for the program at the appropriate location.
		Also flush the TLB so we can switch between programs smoothly.*/

	//allocate a 4MB page of memory for thie process
	Page_Directory[STARTMB/PDOFFSETMB] = PHYSSTARTADDR + curPid * PHYSADDROFFSET;
	//set flags to appropriate values
	Page_Directory[STARTMB/PDOFFSETMB] |= ((1 << PRESENT) | (1 << RW) | (1 << US) | (1 << PCD) | (1 << PS));

		//reload cr3 to flush tlb
    asm volatile ("movl %%eax, %%CR3"
                  :
                  : "a" (Page_Directory)
                  );

	/*Step 6: Load a program onto the page we just allocated. Also update local pid
		values in order to keep track of which program is running and set new process
		to active.*/

	//load the program onto the page
	program_load(filename);

	//update pcb's parent pid value
	curPCB->parentPid = currentPid;

	//update current pid value
	currentPid = curPid;

	//set new process to active
	activeProcess[curPid] = ACTIVE;

	/* Step 7: Perform the context switch to start the user-level program.*/

	asm volatile("movl %%esp, %0			\n"			/*save parent's esp and ebp*/
				 "movl %%ebp, %1			\n"
				 : "=r"(curPCB->parentESP), "=r"(curPCB->parentEBP)
				 );

	//save ss0 and esp0
	tss.esp0 = KERNEL_ADDR_END - (STACK_SIZE_KERNEL *curPid)-LONG; // take stack size end of kernel
	tss.ss0 = KERNEL_DS;

	asm volatile(
            "movl %2, %%eax      \n"			/*set DS*/
            "movw %%ax, %%ds     \n"
            "pushl %2           \n"     /* Push USER_DS */
            "pushl %3           \n"     /* Push user ESP */
            "pushfl             \n"
            "popl %%eax         \n"
            "orl $0x200, %%eax  \n"     /* Enable Interrupt Flag (bitmask 0x200 sets IF to 1) */
            "pushl %%eax        \n"     /* Push EFLAG with interrupts enabled */
            "pushl %1           \n"     /* Push USER_CS */
            "pushl %0           \n"     /* Push EIP */
            "iret               \n"		 /*iret*/
            :
            : "r"(curPCB->startAddress), "r"(USER_CS), "r"(USER_DS), "r"(PROGRAM_END_ADDR - LONG)
						: "eax"
					);


	//will never run this return statement, but compiler will complain otherwise (0 is arbitrary)
	return 0;
}

/* read
 * Description: handler for read system call (reads from device or file)
 * Inputs: file descriptor number, buffer to read into, number of bytes to read
 * Outputs: Nothing
 * Returns: depends on type of file/device read (see appropriate handler for details)
 * Side Effects: see appropriate handler for details
 */
int32_t read (int32_t fd, void* buf, int32_t nbytes){
	sti();			//restore interrupts so all read calls work
	//file descriptor 1 (stdout) is write only
	if(fd == 1)
	{
		return -1;//return an error
	}
	//if file descriptor number is invalid, return an error
	if( fd < FD_MIN || fd > FD_MAX){
		return -1;
	}

	//null check
	if(buf == NULL)
	{
			return -1;//return an error
	}

	//get the current process's pcb
	pcb_t* curPCB = get_pcb();
	//to check if it's being used
	if(curPCB->file_descriptor[fd].flags == 0){
		return -1;//return an error
	}
	return (((curPCB->file_descriptor)[fd].filename_operations[READ])(fd, buf, nbytes));//call the appropriate read function and return it's output
	return 0;//so the compiler doesn't complain (0 is arbitrary)
}

/* write
 * Description: handler for write system call (writes to device - writes to files are invalid)
 * Inputs: file descriptor number, buffer to write from, number of bytes to write
 * Outputs: Nothing
 * Returns: depends on type of file/device write (see appropriate handler for details)
 * Side Effects: see appropriate handlers for details
 */
int32_t write (int32_t fd, const void* buf, int32_t nbytes){
	//file descriptor 0 (stdout) is read only
	if(fd == 0)
	{
		return -1;//return an error
	}

	//null check
	if(buf == NULL)
	{
		//return an error
		return -1;
	}

	//if file descriptor number is invalid, return an error
	if( fd < FD_MIN || fd > FD_MAX){
		return -1;
	}
	//get the current process's pvb
	pcb_t* curPCB = get_pcb();
	//to check if it's being used
	if(curPCB->file_descriptor[fd].flags == 0){
		return -1;//return an error
	}
	return (((curPCB->file_descriptor)[fd].filename_operations[WRITE])(fd, buf, nbytes));//call the appropriate write function and return it's output
	return 0;//so the compiler doesn't complain (0 is arbitrary)
}

/* open
 * Description: handler for open system call (opens file descriptor for file or device)
 * Inputs: file to open
 * Outputs: Nothing
 * Returns: file descriptor number if successful, -1 otherwise
 * Side Effects: opens file and file descriptor
 */
int32_t open (const uint8_t* filename){
	dentry_t dentry_curr;																												//will hold dentry of file we are opening
    int32_t fd_c;																																//will hold file descriptor number we are returning
    fd_c = -1;																																	//initialized to -1 to indicate error if file descriptor is not found
    pcb_t* curPCB = get_pcb();																									//get the current process's pcb

		//if no filename is given, return an error
    if(filename == NULL){
        return -1;
    }

		//if file is not found, return an error
    if(read_dentry_by_name(filename, &dentry_curr) == -1){
        return -1;
    }

    int i;																																			//loop counter
		//find first nonzero file descriptor
		for(i =0; i<= FD_MAX; i++){
        if(curPCB->file_descriptor[i].flags == 0){
            fd_c = i;
            break;
        }
    }

		//if file descriptor is not found, return -1
    if(fd_c == -1 || fd_c > FD_MAX || fd_c < FD_MIN){
        return -1;
    }

		//set the file descriptor appropriately according to file type
    switch(dentry_curr.filetype){
        case 0:
						//set file descriptor values to rtc values
            curPCB->file_descriptor[fd_c].filename_operations = rtc_operations;
						curPCB->file_descriptor[fd_c].inode_num = dentry_curr.inodenum;			//irrelevant to rtc driver
						curPCB->file_descriptor[fd_c].file_position = 0;										//irrelevant to rtc file
						curPCB->file_descriptor[fd_c].flags = 1;														//mark as present
						((curPCB->file_descriptor)[fd_c].filename_operations[OPEN])(filename, fd_c);//call the open function of the corresponding file_type
						return fd_c;																												//return file descriptor number

        case 1:
						//set file descriptor values to directory values
            curPCB->file_descriptor[fd_c].filename_operations = directory_operations;
						curPCB->file_descriptor[fd_c].inode_num = dentry_curr.inodenum;			//irrelevant to directory driver
						curPCB->file_descriptor[fd_c].file_position = 0;										//start with no offset
						curPCB->file_descriptor[fd_c].flags = 1;														//mark as present
						((curPCB->file_descriptor)[fd_c].filename_operations[OPEN])(filename, fd_c);	//call the open function of the corresponding file_type
						return fd_c;																												//return file descriptor number

        case 2:
						//set file descriptor values to file values
            curPCB->file_descriptor[fd_c].filename_operations = file_operations;
						curPCB->file_descriptor[fd_c].inode_num = dentry_curr.inodenum;
						curPCB->file_descriptor[fd_c].file_position = 0;//start with no offset
						curPCB->file_descriptor[fd_c].flags = 1;														//mark as present
						((curPCB->file_descriptor)[fd_c].filename_operations[OPEN])(filename); //call the open function of the corresponding file_type
						return fd_c;																												//return file descriptor number

        default :
						//if file type number is invalid, return an error
            return -1;
    }

}

/* close
 * Description: handler for close system call (closes file descriptor for file or device)
 * Inputs: file descriptor number
 * Outputs: Nothing
 * Returns: 0 if successful, -1 otherwise
 * Side Effects: closes file and file descriptor
 */
int32_t close (int32_t fd) {

	//if file descriptor cannot be closed, return an error
	if( fd < FD_MIN_CLOSE || fd > FD_MAX){
		return -1;
	}
	pcb_t* curPCB = get_pcb();																										//get the current process's pcb
	//to check if it's being used
	if(curPCB->file_descriptor[fd].flags == 0){
		return -1;//return an error
	}
	//resetting file descriptor
	(curPCB->file_descriptor)[fd].file_position = 0;
	(curPCB->file_descriptor)[fd].flags = 0;
	//return the output of appropriate close function
	return (((curPCB->file_descriptor)[fd].filename_operations[CLOSE])(fd));
	return 0;//so the compiler doesn't complain
}

/* getargs
 * Description: handler for getargs system call (gets the arguments given to execute)
 * Inputs: buffer to write to, max number of bytes to write
 * Outputs: Nothing
 * Returns: 0 if successful, -1 otherwise
 * Side Effects: Sets up IDT
 */
int32_t getargs (uint8_t* buf, int32_t nbytes) {
	pcb_t* curPCB = get_pcb();																										//get the pcb for the current process

	//null check
	if(buf == NULL)
	{
		//return an error
		return -1;
	}

	//check whether or not pointer is valid by checking whether or not it fits on the user page (if not valid, return an error)
	if((buf < (uint8_t*)USERPGSTART) || ((buf + nbytes) > (uint8_t*)USERPGEND))
	{
			return -1;//return an error
	}

	//if no arguments, return an error(first character is null character)
	if(curPCB->args[0] == '\0')
	{
		return -1;//return an error
	}

	//if not enough space for arguments and null terminating character (+1) in given buffer, return an error
	if((strlen((int8_t*)curPCB->args) + 1) > nbytes)
	{
		return -1;
	}

	//copy arguments into buffer(including null terminating character at the end)
	memcpy(buf, curPCB->args, (strlen((int8_t*)curPCB->args) + 1));
	//return successfully
	return 0;
}

/* vidmap
 * Description: handler for vidmap system call (maps video memory into user memory)
 * Inputs: memory location to write to
 * Outputs: Nothing
 * Returns: 0 if successful, -1 otherwise
 * Side Effects: maps user memory location to video memory
 */
int32_t vidmap (uint8_t** screen_start) {
	//null check
	if(screen_start == NULL)
	{
		//return an error
		return -1;
	}

	//if pointer given is invalid (falls outside the bounds of the user page) return an error
	if((screen_start < (uint8_t**)USERPGSTART) || (screen_start > (uint8_t**)USERPGEND))
	{
			return -1;
	}

	pcb_t* curPCB = get_pcb();
	curPCB->vidmapped = 1;
	map_vid_mem(curPCB->pid);
	//set the screen_start variable given to the appropriate virtual address
	*screen_start = (uint8_t*)(USERVIDSTART);

	//return success
	return 0;
}

/* set_handler
 * Description: handler for set_handler system call(sets a handler for a given signal)
 * Inputs: signal number, address of handler
 * Outputs: Nothing
 * Returns: 0 if handler was sucessfully set, -1 otherwise
 * Side Effects: Nothing
 */
int32_t set_handler(int32_t signum, void* handler_address)
{
	//if signum is not valid, return an error(less than system call with lowest value -- DIV_ZERO -- or greater than system call with largest value -- USER1)
	if((signum < DIV_ZERO) || (signum > USER1))
	{
		return -1;
	}

	//get the current PCB
	pcb_t* curPCB = get_pcb();

	//if handler address is null, reset handler to default
	if(handler_address == NULL)
	{
		//if one of first 3 signals, set kill_task handler, otherwise set ignore handler
		if((signum == DIV_ZERO) || (signum == SEGFAULT) || (signum == INTERRUPT))
		{
			curPCB->signal_handlers[signum] = kill_task;
		}
		else
		{
			curPCB->signal_handlers[signum] = ignore;
		}
		//return success
		return 0;
	}

	//if pointer does not point to valid user level page address, return an error
	if((handler_address < (void*)USERPGSTART) || (handler_address > (void*)USERPGEND))
	{
		return -1;
	}

	//set the appropriate handler entry to the user level address
	(curPCB->signal_handlers)[signum] = handler_address;

	//return success
	return 0;
}

/* sigreturn
 * Description: handler for sigreturn system call
 * Inputs: esp from before the function is called
 * Outputs: Nothing
 * Returns: hardware context's eax value
 * Side Effects: modifies stack
 */
int32_t sigreturn(void * esp)
{
	//get the current process's pcb
	pcb_t* curPCB = get_pcb();

	//unmask all signals
	memset((curPCB->masks), 0, NUM_SIGNALS * LONG);

	//unblock signals from being executed
	curPCB->sigBlocking = UNBLOCKED;

	hardware_context_t* hardwareContext = (hardware_context_t*)(curPCB->userESP + (2 * LONG));//points to hardware context saved on user level stack (2 long integers below the user esp we saved -- see docs for image of user-level stack)

	system_call_sigreturn_stack_t* curStack; //used to modify the current stack into the new stack

	//get pointer to current stack so we can modify it
	curStack = (system_call_sigreturn_stack_t*)esp;

	//put values from hardware context or current stack into new stack struct
	curStack->EBX = hardwareContext->ebx;
	curStack->ECX = hardwareContext->ecx;
	curStack->EDX = hardwareContext->edx;
	curStack->ESI = hardwareContext->esi;
	curStack->EDI = hardwareContext->edi;
	curStack->EBP = hardwareContext->ebp;
	curStack->FS = (int16_t)((hardwareContext->fs) && WORD_MASK);
	curStack->ES = (int16_t)((hardwareContext->es) && WORD_MASK);
	curStack->DS = (int16_t)((hardwareContext->ds) && WORD_MASK);
	curStack->userRetAddr = hardwareContext->returnAddr;
	curStack->CS = hardwareContext->cs;
	curStack->EFLAGS = hardwareContext->eflags;
	curStack->userESP = hardwareContext->esp;
	curStack->SS = hardwareContext->ss;

	//return hardware context's eax value(to avoid clobbering of eax)
	return hardwareContext->eax;
}


/* get_pcb
 * Description: gets the pcb for the given process(helper function)
 * Inputs: pid(behavior undefined if pid is invalid)
 * Outputs: Nothing
 * Returns: Appropriate PCB
 * Side Effects: Nothing
 */
pcb_t* get_pcb() {
	pcb_t* pcb; 																																	//will hold pcb we are returning

	//anding esp with 0xFFFFE000 gets us to top of kernel stack (last 13 bits are zeroed out because 8kB is 2^13)
	asm volatile ("movl %%esp, %%eax \n\t"
          "andl $0xFFFFE000, %%eax \n\t"
          : "=a"(pcb)
          );
	return pcb;//return the appropriate pcb
}

/* setup_pcb
 * Description: initializes the pcb in kernel memory
 * Inputs: Nothing
 * Returns: Nothing
 * Outputs: Nothing
 * Side effects: sets up the pcb with STDIN and STDOUT
 */
void setup_pcb(uint32_t pid) {
	//place the pcb in memory at the top of the kernel stack
	pcb_t* pcb = (pcb_t*)(KERNEL_ADDR_END - (pid + 1) * STACK_SIZE_KERNEL);//+1 corresponds with the fact that we want the top of the appropriate kernel stack
	//clear out any data that may be left in memory
	memset(pcb,0,PCB_SIZE);
	//map process to terminal it's on and place pid in terminal struct at the appropriate locations
	if (pid<=2) {
		pcb->terminal = terminals[pid];
		(terminals[pid])->PID[((terminals[pid])->num_processes)++] = pid;
	} else {
		pcb->terminal = active_terminal;
		active_terminal->PID[(active_terminal->num_processes)++] = pid;
		active_terminal->active_process_pid = pid;
	}
	//setup fd 0 and 1 to be stdin and stdout
	(pcb->file_descriptor)[0].filename_operations = terminal_operations;
	(pcb->file_descriptor)[1].filename_operations = terminal_operations;
	//set bit 0 of flags to mark each fd as present
	(pcb->file_descriptor)[0].flags |= 0x00000001;
	(pcb->file_descriptor)[1].flags |= 0x00000001;

	//set jump table of signal handlers
	(pcb->signal_handlers)[DIV_ZERO] = kill_task;
	(pcb->signal_handlers)[SEGFAULT] = kill_task;
	(pcb->signal_handlers)[INTERRUPT] = kill_task;
	(pcb->signal_handlers)[ALARM] = ignore;
	(pcb->signal_handlers)[USER1] = ignore;

	//initialize masks for signals (0 corresponds with unmasked)
	(pcb->masks)[DIV_ZERO] =  0;
	(pcb->masks)[SEGFAULT] =  0;
	(pcb->masks)[INTERRUPT] =  0;
	(pcb->masks)[ALARM] =  0;
	(pcb->masks)[USER1] =  0;

	//initialize signal blocking flag
	pcb->sigBlocking = UNBLOCKED;

	//set num_pending for signals(0 corresponds to 0 elements currently in array)
	pcb->num_pending = 0;
}

/* map_vid_mem
 * Description: helper function for vidmap
 * Inputs: pid of process to map video memory for
 * Returns: Nothing
 * Outputs: Nothing
 * Side effects: remaps video memory
 */
void map_vid_mem(int pid){
	//get current pcb
	pcb_t* curPCB = (pcb_t*)(KERNEL_ADDR_END - (STACK_SIZE_KERNEL * (pid + 1)));//+1 corresponds with the fact that we want the top of the appropriate kernel stack
	//get current process's terminal
	terminal_t* t = curPCB->terminal;
	//remap entry 0 of page table 2 to remap video memory
	Page_Table_2[0] = (int32_t)(t->vid_buffer) | (1 << RW) | (1 << US);
	//if video memory is mapped correctly, mark page as present
	if (curPCB->vidmapped)
		Page_Table_2[0] |= (1 << PRESENT);
}

/* sendSignal
 * Description: sends the given signal
 * Inputs: number of signal to send, process to send it to
 * Returns: Nothing
 * Outputs: Nothing
 * Side effects: sends a signal
 */
void sendSignal(int signum, int32_t pid)
{
	#if SIGNALS_ENABLED
	//print message indicating signal is being sent
	//printk(pid, "Signal %d being sent to process %d\n", signum, pid);

	//get the pcb of the given process
	pcb_t* curPCB = (pcb_t*)(KERNEL_ADDR_END - (STACK_SIZE_KERNEL * (pid + 1)));//+1 corresponds with the fact that we want the top of the appropriate kernel stack

	//if signal is masked, do nothing and return
	if(curPCB->masks[signum] == 1)//1 corresponds with masked
	{
		return;
	}

	//if pending array is full, do nothing and return
	if(curPCB->num_pending == MAX_SIGNALS)
	{
		return;
	}

	//otherwise, add signal to end of pending array and increment number of pending signals
	curPCB->pending[curPCB->num_pending] = signum;
	curPCB->num_pending++;
	#endif

	//return
	return;
}

/* send_signal
 * Description: sends the user1 signal to the given process
 * Inputs: number of signal to send
 * Returns: 0 on success, -1 on failure
 * Outputs: Nothing
 * Side effects: sends a signal
 */
int32_t send_signal(int32_t pid)
{
	#if SIGNALS_ENABLED
	//if signals are enabled, send the user1 signal to the given program and then return success
	sendSignal(USER1, pid);
	return 0;
	#else
	//if signals are disabled, return an error
	return -1;
	#endif
}

/* get_pids
 * Description: gets the current process's pid
 * Inputs: Nothing
 * Returns: current pid
 * Outputs: Nothing
 * Side effects: Nothing
 */
int32_t get_pids(uint8_t* filename, uint32_t* buf, int32_t nbytes)
{
	#if SIGNALS_ENABLED
	int i;//loop counter
	int32_t nPids = nbytes / 4;//holds the number of pids to copy(each pid is 4 bytes long)
	pcb_t* curPCB;//will hold the pcb of the process we are dealing with
	int32_t pidsCopied = 0;//will hold number of pids copied(initialized to 0 pids copied)

	//if either filename or buf is null, return an error
	if((filename == NULL) || (buf == NULL))
	{
		return -1;
	}

	//if filename given is invalid (falls outside the bounds of the user page) return an error
	if((filename < (uint8_t*)USERPGSTART) || ((filename + SIZE_NAME) > (uint8_t*)USERPGEND))
	{
			return -1;
	}

	//if buffer given is invalid (falls outside the bounds of the user page) return an error
	if((buf < (uint32_t*)USERPGSTART) || ((buf + nPids) > (uint32_t*)USERPGEND))
	{
			return -1;
	}

	//cycle through all processes and stop if we can't copy any more pids
	for(i = 0; (i < MAX_PROCESSES) && (nPids > 0); i++)
	{
		//get the pcb of the ith process
		curPCB = (pcb_t*)(KERNEL_ADDR_END - (STACK_SIZE_KERNEL * (i + 1)));
		//if process exists and is active and has the same name as the filename given, ...
		if((pidAvailable[i] == NOT_AVAILABLE) && (activeProcess[i] == ACTIVE) && (strncmp((int8_t*)filename,(int8_t*)(curPCB->name), SIZE_NAME) == 0))//0 corresponds with strings being the same
		{
			//copy the pid into the buffer, increment number of pids copied, and decrement the amount of pids to copy
			buf[pidsCopied++] = i;
			nPids--;
		}
	}

	//if signals are enabled, return the number of pids copied
	return pidsCopied;
	#else
	//if signals are disabled, return an error
	return -1;
	#endif
}
