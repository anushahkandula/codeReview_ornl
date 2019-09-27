#ifndef _SYS_CALLS_H
#define _SYS_CALLS_H

#include "keyboard.h"
#include "terminal.h"
#include "types.h"
#include "lib.h"
#include "file_sys.h"
#include "handlers.h"

//flags for 3 arrays below
#define AVAILABLE 			0
#define NOT_AVAILABLE		1
#define INACTIVE				0
#define ACTIVE					1
#define RUN							1
#define NOT_RUN					0

/* see docs for definitions of system calls 1-10
 * System call 11 allows the user to send the user1 signal to the given process
 * Prototype:
 * 11. send_signal(int32_t pid);
 * System call 12 copies all of the pids for active processes with the same name
 * into a user-level buffer until nbytes is reached or it runs out of processes
 * with that name. nbytes is rounded to the largest multiple of 4 less than the
 * given value. It returns the number of pids copied.
 * Prototype:
 * 12. get_pids(uint8_t* filename, uint32_t* buf, int32_t nbytes);
 */

//used to toggle signals on and off(alarm signal can only be sent when rtc virtualization is on -- see RTC.h)
#define SIGNALS_ENABLED     1

#define FILE_DESC_NUM       8//number of file descriptors per process that the documentation requires us to support
#define PCB_SIZE 172         //size of the PCB in bytes

#define MAX_PROCESSES				6//maximum number of processes that can be run currently

//magic number used to indicate that no process exists
#define NO_PROCESS 					-1

//Offset for virtual address of program in MB
#define STARTMB							128

//Offset per entry in Page Directory in MB
#define PDOFFSETMB					4

//Physical address of first program (in bytes)
#define PHYSSTARTADDR				(8 << 20)

//offset of physical address for subsequent programs (in bytes)
#define PHYSADDROFFSET			(4 << 20)

#define FD_MAX							7
#define FD_MIN							0
#define FD_MIN_CLOSE				2

//Magic numbers used to denote an executable file
#define MAGIC1							0x7f
#define MAGIC2							0x45
#define MAGIC3							0x4c
#define MAGIC4							0x46

//appropriate bit shift values to get to a certain byte of a 32 bit value (byte size is 8 bits)
#define BYTE1								8 * 3
#define BYTE2								8 * 2
#define BYTE3								8 * 1
#define BYTE4								8 * 0

//macros for jump table offsets
#define OPEN 	0
#define READ 	1
#define WRITE   2
#define CLOSE   3

//bounds of user page
#define USERPGSTART					(STARTMB << 20)
#define USERPGEND						((STARTMB + PDOFFSETMB) << 20)

//possible values for blocking, toClear and reading flags
#define BLOCKED           1
#define UNBLOCKED         0
#define READING           1
#define NOT_READING       0
#define CLEAR             1
#define NO_CLEAR          0

//Offsets within kernel page
#define KERNEL_ADDR_END (8 << 20)
#define STACK_SIZE_KERNEL (8 << 10)
#define LONG 4 //size of 32 bit integer in bytes
#define PROGRAM_END_ADDR ((128 + 4) << 20)

//start of user level video memory
#define USERVIDSTART				USERPGEND
#define USERVIDSTARTMB			(STARTMB + PDOFFSETMB)

//number of signals we have implemented
#define NUM_SIGNALS         5

//maximum number of pending signals (we allow 2 times the number of signals to be pending (2 is arbitrary))
#define MAX_SIGNALS         2 * NUM_SIGNALS

//Signals and their respective number
#define DIV_ZERO            0
#define SEGFAULT            1
#define INTERRUPT           2
#define ALARM               3
#define USER1               4

//mask to get word from long
#define WORD_MASK           0x0000FFFF

//size of sigreturn system call stack(number of elements in struct times size of each element in bytes)
#define SIGRETSC_STACK_SIZE ((11 * LONG) + (3 * WORD))

/* (all stack entries below are 4 bytes unless otherwise indicated) (Parameters are unused for this system call but are pushed by asm linkage)
Stack at/below esp before function is called if it is called from system call assembly linkage
+------------------+
|        EBX       |
+------------------+
|        ECX       |
+------------------+
|        EDX       |
+------------------+
|        ESI       |
+------------------+
|        EDI       |
+------------------+
|    kernel EBP    |
+------------------+
|   FS (2 Bytes)   |
+------------------+
|   ES (2 Bytes)   |
+------------------+
|   DS (2 Bytes)   |
+------------------+
|return address for|
|user-level program|
+------------------+
|    0   |   CS    |
+------------------+
|      EFLAGS      |
+------------------+
|  user-level ESP  |
+------------------+
|    0   |   SS    |
+------------------+
*/

//used to create stack for return from sigreturn handler
typedef struct __attribute__((packed)) {
  int32_t EBX;
  int32_t ECX;
  int32_t EDX;
  int32_t ESI;
  int32_t EDI;
  int32_t EBP;
  int16_t FS;
  int16_t ES;
  int16_t DS;
  int32_t userRetAddr;
  int32_t CS;
  int32_t EFLAGS;
  int32_t userESP;
  int32_t SS;
}system_call_sigreturn_stack_t;


typedef int32_t (*function)();
typedef void (*functionVoid)();

//holds whether or not process id is available (implicity counts number of processes)
extern int32_t pidAvailable[MAX_PROCESSES];

//holds whether or not the process is currently active (pid = index)
extern int32_t activeProcess[MAX_PROCESSES];

//holds whether or not the process has been run in this cycle
extern int32_t beenRun[MAX_PROCESSES];

//holds currently running process (initialized t0 10 to indicate no process)
extern int32_t currentPid;

//defines the file descriptor structure(same as described in mp documentation)
typedef struct {
    function* filename_operations;
    uint32_t inode_num;
    uint32_t file_position;
    //use The LSB of flags as a present bit
    uint32_t flags;
} file_descriptor_t;

//defines the pcb
typedef struct {
	file_descriptor_t file_descriptor[FILE_DESC_NUM];//defines file descriptor array as described in documentation
  uint8_t name[SIZE_NAME];//stores the name given to the program
  uint8_t args[BUFFER_SIZE]; //stores the arguments given to the program (args cannot be longer than size of buffer)
  int32_t sigBlocking;//used to run signal handlers atomically with respect to each other
  int32_t pid;//current process's process id
  terminal_t* terminal; //pointer to the terminal associated with the process
  int32_t vidmapped; // boolean of whether or not video memory is mapped
  uint32_t* startAddress;//start address of executable file
  int32_t parentPid;//parent's pid
  uint32_t parentESP;//parent's esp
  uint32_t parentEBP;//parent's ebp
  uint32_t curEBP;//current process's ebp(used for scheduling and signals)
  uint32_t curESP;//current process's esp(used for scheduling and signals)
  uint32_t curTSSesp0;//current process's TSS esp0 value(used for scheduling and signals)
  uint16_t curTSSss0;//current process's TSS ss0 value(used for scheduling and signals)
  int32_t masks[NUM_SIGNALS];//used to mask certain signals
  int32_t pending[MAX_SIGNALS];//holds signals that are pending
  int32_t num_pending;//used to count the number of signals pending
  uint32_t userESP;//user esp from when signal handler was called
  //jump table of signal handlers used when delivering a signal
  void (*signal_handlers[NUM_SIGNALS])();
} pcb_t;

//handler for halt system call (halts user level program)
int32_t halt (uint32_t status);

//handler for execute system call (executes user level program)
int32_t execute (const uint8_t* command);

//handler for read system call (reads from device or file)
int32_t read (int32_t fd, void* buf, int32_t nbytes);

//handler for write system call (writes to device - writes to files are invalid)
int32_t write (int32_t fd, const void* buf, int32_t nbytes);

//handler for open system call (opens file descriptor for file or device)
int32_t open (const uint8_t* filename);

//handler for close system call (closes file descriptor for file or device)
int32_t close (int32_t fd);

//handler for getargs system call (gets the arguments given to execute)
int32_t getargs (uint8_t* buf, int32_t nbytes);

//handler for vidmap system call (maps video memory into user memory)
int32_t vidmap (uint8_t** screen_start);

//handler for set_handler system call(sets a handler for a given signal)
int32_t set_handler(int32_t signum, void* handler_address);

//handler for sigreturn system call
int32_t sigreturn(void * esp);

//handler for send_signal system call(returns failure if signals are disabled)
int32_t send_signal(int32_t pid);

//handler for get_pid system call(return failure if signals are disabled)
int32_t get_pids(uint8_t* filename, uint32_t* buf, int32_t nbytes);

//gets the pcb for the given process
pcb_t* get_pcb();

//initializes the pcb for the given process
void setup_pcb(uint32_t pid);

//helper function for vidmap
void map_vid_mem(int pid);

//sends the given signal to the given process
void sendSignal(int signum, int pid);
#endif
