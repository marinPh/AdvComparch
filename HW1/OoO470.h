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

typedef struct {
    char opcode[OPCODE];
    int dest;
    int src1;
    int src2;
} InstructionEntry;

typedef struct {
    InstructionEntry *instructions;
    size_t size;
} Instruction;

extern unsigned int PC;
extern unsigned long PhysRegFile[REGS];
extern struct {
    unsigned int *DIRarray;
    unsigned int DIRSize;
} DIR;
extern bool exception;
extern unsigned int ePC;
extern bool backPressureRDS;
extern unsigned int RegMapTable[ENTRY];
extern unsigned int *FreeList[ENTRY];
extern bool BusyBitTable[REGS];

typedef struct {
    bool Done;
    bool Exception;
    int LogicalDestination;
    int OldDestination;
    int PC;
} ActiveListEntry;

extern struct {
    ActiveListEntry ALarray[ENTRY];
    int ALSize;
} ActiveList;

typedef struct {
    int reg;
    int value;
} forwardingTableEntry;

extern struct {
    forwardingTableEntry table[ENTRY];
    int size;
} forwardingTable;

typedef struct {
    int DestRegister;
    bool OpAIsReady;
    int OpARegTag;
    int OpAValue;
    bool OpBIsReady;
    int OpBRegTag;
    int OpBValue;
    char OpCode[OPCODE];
    int PC;
} IntegerQueueEntry;

extern struct {
    IntegerQueueEntry IQarray[ENTRY];
    int IQSize;
} IntegerQueue;
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
int PopFreeList();
void initALU();
void initForwardingTable();

#endif /* MIPS_SIMULATOR_H */
