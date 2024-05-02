#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cJSON.h"
#include "cJSON_Utils.h"

#define OPCODE 5

typedef enum {
    ADD, ADDI, SUB, MOV, MULU, LD, ST, LOOP, LOOP_PIP, NOP  
} InstructionType;

// Instructions
typedef struct {
    char opcode[OPCODE]; // 4 characters + null terminator
    unsigned int block;  // BB0, BB1, BB2
    int dest;
    int src1;            // Used as source register or as an immediate value, depending on the instruction type
    int src2;            // Used only by register to register operations (e.g., add, sub, mulu)
    int imm;             // Immediate value for immediate operations (e.g., addi, ld, mov with immediate)
    int predicate;       // For conditional execution; true or false for LOOP types, ignored otherwise => modifies processor state only if true, else discarded at commit
    InstructionType type;// Type of the instruction to handle different formats
    int cycle;           // Cycles the instruction has been in the execution stage
    bool done;           // True if the instruction has been executed
} InstructionEntry;

void parseString(char *instr_string, InstructionEntry *entry);


#endif /* UTILS_H */