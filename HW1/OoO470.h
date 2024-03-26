#ifndef OoO470_H
#define OoO470_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "lib/cJSON.h"
#include "lib/cJSON_Utils.h"

#define REGS 64
#define ENTRY 32
#define INSTR 4
#define OPCODE 5

#define BEGINLOG 0
#define ENDLOG 1
#define LOGCOMMA 2
#define LOG 3

// Structure for parsing JSON entry
typedef struct {
    char opcode[OPCODE]; // 4 characters + null terminator
    int dest;
    int src1;
    int src2;
} InstructionEntry;

// Structure for parsing JSON
typedef struct {
    InstructionEntry *instructions;
    size_t size;
} Instruction;

// Program Counter unsigned integer pointing to the next instruction to fetch.
extern unsigned int PC;
// Physical Register File
extern unsigned long PhysRegFile[REGS]; // 64 registers

// Decoded Instruction Register
extern struct {
    unsigned int *DIRarray; // array that buffers instructions that have been decoded but have not been renamed and dispatched yet
    unsigned int DIRSize;   // size of the DIR
} DIR;

// Exception Flag
extern bool exception;
// Exception PC
extern unsigned int ePC;

extern bool backPressureRDS;
extern unsigned int RegMapTable[ENTRY];
extern bool BusyBitTable[REGS];

// Define a struct for a node in the double linked list
typedef struct FreeListNode {
    int value; // The value of the register
    struct FreeListNode *prev; // Pointer to the previous node
    struct FreeListNode *next; // Pointer to the next node
} FreeListNode;

// FreeList
typedef struct {
    FreeListNode *head; // Pointer to the head of the list
    FreeListNode *tail; // Pointer to the tail of the list
} FreeList;

extern FreeList freeList;

// Entry in the Active List
typedef struct {
    bool Done;
    bool Exception;
    int LogicalDestination; // the architectural register that the instruction writes to
    int OldDestination;
    int PC;
} ActiveListEntry;

// Active List
// instructions that have been dispatched but have not yet completed, renamed instruction
extern struct {
    ActiveListEntry ALarray[ENTRY];
    int ALSize;
} ActiveList;

// Forwarding Table
typedef struct {
    int reg;   // the physical register that the value is forwarded to
    int value; // the value that is forwarded
} forwardingTableEntry;

extern struct {
    forwardingTableEntry table[INSTR];
    int size;
} forwardingTable;

// Entry in Integer Queue
typedef struct {
    int DestRegister;
    bool OpAIsReady;
    int OpARegTag; // for cheking forwarding
    int OpAValue;
    bool OpBIsReady;
    int OpBRegTag;
    int OpBValue;
    char OpCode[OPCODE]; // 4 characters + null terminator
    int PC;
} IntegerQueueEntry;

// Integer Queue
// always 32 entries myx but can be less, need to check if it is full
extern struct {
    IntegerQueueEntry IQarray[ENTRY];
    int IQSize;
} IntegerQueue;

typedef struct {
    IntegerQueueEntry instr;
} ALUEntry;

ALUEntry ALU1[INSTR];
ALUEntry ALU2[INSTR]; // TODO 4 ALUs not 2 => max 4 instructions

void showPhysRegFile();
void showActiveList();
void showFreeList();
void showIntegerQueue();
void showDIR();
void showRegMapTable();
void showBusyBitTable();
void showInstruction();
int parser(char* file_name);
void Commit();
void Exception();
bool isOpBusy(int reg);
bool forwardable(int reg);
void FetchAndDecode();
void RDS();
void Issue();
void Execute();
void showBp();
void showALU();
void showForwardingTable();

void init();

void outputSystemStateJSON(FILE *file); 
int log(char *output_file, int mode);

void propagate();

#endif /* MIPS_SIMULATOR_H */
