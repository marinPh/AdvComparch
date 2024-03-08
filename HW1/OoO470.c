#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#define REGS 64
#define ENTRY 32

// Program Counter unsigned integer pointing to the next instruction to fetch.
unsigned int PC = 0;

// Physical Register File
unsigned long PhysRegFile[REGS]; // 64 registers of 64 bits each

// Decoded Instruction Register
unsigned int * DIR; // array that buffers instructions that have been decoded but have not been renamed and dispatched yet

// Exception Flag
bool exception = false;

// Exception PC
unsigned int ePC = 0;

// Register Map Table
unsigned int * RenameTable; // array that maps architectural register names to physical register names
// 32 architectural registers, 64 physical registers

// Free List
unsigned int * FreeList; // array that keeps track of the physical registers that are free
// on initialization 32-63 are free

// Busy Bit Table
bool BusyBitTable[REGS] = {false}; // whether the value of a specific physical register will be generated from the Execution stage

// Entry in the Active List
typedef struct {
    bool Done;
    bool Exception;
    int LogicalDestination;
    int OldDestination;
    int PC;
} ActiveListEntry;

// Active List
// instructions that have been dispatched but have not yet completed
// renamed instructions
ActiveListEntry ActiveList[ENTRY]; 
// Entry in Integer Queue
typedef struct {
    int DestRegister;
    bool OpAIsReady;
    int OpARegTag; // for cheking forwarding 
    int OpAValue;
    bool OpBIsReady;
    int OpBRegTag;
    int OpBValue;
    char OpCode[5];  // 4 characters + null terminator
    int PC;
} IntegerQueueEntry;

// Integer Queue
// instructions awaiting issuing
IntegerQueueEntry IntegerQueue[ENTRY]; 