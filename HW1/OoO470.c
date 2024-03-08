#include stdio.h
#include stdlib.h
#include string.h
#include math.h
#include stdbool.h

#define REGS 64

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