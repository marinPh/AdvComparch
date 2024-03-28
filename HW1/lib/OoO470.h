#ifndef OoO470_H
#define OoO470_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "cJSON.h"
#include "cJSON_Utils.h"

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


// Forwarding Table
typedef struct {
    int reg;   // the physical register that the value is forwarded to
    unsigned long value; // the value that is forwarded
    bool exception; // if the instruction that produced the value has an exception
} forwardingTableEntry;


// Entry in Integer Queue
typedef struct {
    int DestRegister;
    bool OpAIsReady;
    int OpARegTag; // for cheking forwarding
    unsigned long OpAValue;
    bool OpBIsReady;
    int OpBRegTag;
    unsigned long OpBValue;
    char OpCode[OPCODE]; // 4 characters + null terminator
    int PC;
} IntegerQueueEntry;



typedef struct {
    IntegerQueueEntry instr;
} ALUEntry;

bool getException();
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
int forwardable(int reg);
void FetchAndDecode();
void RDS();
void Issue();
void Execute();
void showBp();
void showALU();
void showForwardingTable();

void init();

void outputSystemStateJSON(FILE *file); 
int slog(char *f_out, int i);

void propagate();

#endif /* MIPS_SIMULATOR_H */
