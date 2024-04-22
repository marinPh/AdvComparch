#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "lib/cJSON.h"
#include "lib/cJSON_Utils.h"
#include "lib/VLIW470.h"

#define REGS 96
// #define OPCODE 5

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

typedef struct
{
    char ID;
    unsigned int reg;
} dependency;
typedef struct
{
    dependency *list;
    unsigned int size;
} idList;

void pushId(idList *list, char id, unsigned int reg)
{
    list->list = realloc(list->list, (list->size + 1) * sizeof(dependency));
    list->list[list->size].ID = id;
    list->list[list->size].reg = reg;
    list->size++;
}

typedef struct
{
    unsigned int address;
    char ID;
    unsigned int dest;
    idList local;
    idList loop;
    idList invariant;
    idList postL;
} dependencyEntry;

typedef struct
{
    dependencyEntry *dependencies;
    unsigned int size;
} dependencyTable;

dependencyTable depTable = {NULL, 0};

void pushDependency(dependencyTable *table, dependencyEntry entry)
{
    table->dependencies = realloc(table->dependencies, (table->size + 1) * sizeof(dependencyEntry));
    table->dependencies[table->size] = entry;
    table->size++;
}

dependencyTable dependencyTableInit()
{
    dependencyTable table = {NULL, 0};
    for (int i = 0; i < instrs.size; i++)
    {
        dependencyEntry entry = {i, i, instrs.instructions[i].dest, {0, {NULL, 0}}, {0, {NULL, 0}}, {0, {NULL, 0}}, NULL};
        pushDependency(&table, entry);
    }
    return table;
}
void whatType(int instr1, int instr2, dependencyTable table)
{
    dependencyEntry *entry = &(table.dependencies[instr2]);
    // we check if the dest of instr1 is equal to src1 or src2 of instr2
    if (instrs.instructions[instr1].dest == instrs.instructions[instr2].src1 || instrs.instructions[instr2].src2 == instrs.instructions[instr1].dest)
    {
        // check if in same block
        if (instrs.instructions[instr1].block == instrs.instructions[instr2].block)
        {
            // check if instr1 > instr2
            if (instr1 > instr2)
            {
                pushId(&entry->loop, table.dependencies[instr1].ID, instrs.instructions[instr1].dest);
            }
            else
            {
                pushId(&entry->local, table.dependencies[instr1].ID, instrs.instructions[instr1].dest);
            }
        }
        else
            // check if in post loop means instr1 is in block 1 and instr2 is in block 2
            if (instrs.instructions[instr1].block == 1 && instrs.instructions[instr2].block == 2)
            {
                pushId(&entry->postL, table.dependencies[instr1].ID, instrs.instructions[instr1].dest);
            }
            else
                // check if in invariant
                if (instrs.instructions[instr1].block == 0 && (instrs.instructions[instr2].block == 1 || instrs.instructions[instr2].block == 2))
                {
                    pushId(&entry->invariant, table.dependencies[instr1].ID, instrs.instructions[instr1].dest);
                }
    }
    return;
}

dependencyTable fillDepencies(dependencyTable table)
{
    for (int i = 0; i < table.size; i++)
    {
        // if instruction is in block 2 start from loop start otherwise start from i
        int pot = instrs.instructions[i].dest;
        int start = (instrs.instructions[i].block == 2) ? instrs.loop_start : i;
        for (int j = start; j < instrs.size; j++)
        {
            // check for each instruction if pot is a dest or src
            if (instrs.instructions[j].dest == pot)
            {
                break;
            }

            if (instrs.instructions[j].src1 == pot)
            {
                // if src1 is the same reg as pot then add to dependencies
            }
        }
    }
    return table;
}

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

typedef struct
{
    ALU alu1;
    ALU alu2;
    Mult mult;
    Mem mem;
    Br br;
} VLIW;