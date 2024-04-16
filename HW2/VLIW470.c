#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "lib/cJSON.h"
#include "lib/cJSON_Utils.h"
#include "lib/VLIW470.h"

#define REGS 96
//#define OPCODE 5

// // Instructions
// typedef struct {
//     char opcode[OPCODE]; // 4 characters + null terminator
//     int dest;
//     int src1;
//     int src2;
// } InstructionEntry;

// // Structure for parsing JSON
// typedef struct {
//     InstructionEntry *instructions;
//     size_t size;
// } Instruction;

Instruction instrs = {NULL, 0};

// Program Counter 
unsigned int PC = 0;

// Physical Register File
unsigned long PhysRegFile[REGS]; // 64 registers of 64 bits each

// Predicate Register File
unsigned long PredRegFile[REGS]; 

// Loop Count
unsigned int LC = 0;

// Epilogue Count
unsigned int EC = 0; // TODO can it be neg?

// Register Rotation Base
unsigned int RRB = 0; 

// FUs
typedef InstructionEntry ALU; 
typedef InstructionEntry Mult;
typedef InstructionEntry Mem;
typedef InstructionEntry Br;

// typedef struct {
//     InstructionEntry i; 
// } ALU;

// typedef struct {
//     InstructionEntry i;
// } Mult; 

// typedef struct {
//     InstructionEntry i;
// } Mem;

// typedef struct {
//     InstructionEntry i;
// } Br;

typedef struct {
    ALU alu1;
    ALU alu2;
    Mult mult;
    Mem mem;
    Br br; 
} VLIW;