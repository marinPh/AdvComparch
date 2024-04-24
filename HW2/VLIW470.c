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
void whatType(int instr1, int instr2, dependencyTable *table)
{
    dependencyEntry *entry = &(table->dependencies[instr2]);
    // we check if the dest of instr1 is equal to src1 or src2 of instr2
    if (instrs.instructions[instr1].dest == instrs.instructions[instr2].src1 || instrs.instructions[instr2].src2 == instrs.instructions[instr1].dest)
    {
        if (instrs.loop_start != -1)
        { // check if in same block
            if (instrs.instructions[instr1].block == instrs.instructions[instr2].block)
            {
                // check if instr1 > instr2
                if (instr1 > instr2)
                {
                    pushId(&entry->loop, table->dependencies[instr1].ID, instrs.instructions[instr1].dest);
                }
                else
                {
                    pushId(&entry->local, table->dependencies[instr1].ID, instrs.instructions[instr1].dest);
                }
            }
            else
                // check if in post loop means instr1 is in block 1 and instr2 is in block 2
                if (instrs.instructions[instr1].block == 1 && instrs.instructions[instr2].block == 2)
                {
                    pushId(&entry->postL, table->dependencies[instr1].ID, instrs.instructions[instr1].dest);
                }
                else
                    // check if in invariant
                    if (instrs.instructions[instr1].block == 0 && (instrs.instructions[instr2].block == 1 || instrs.instructions[instr2].block == 2))
                    {
                        pushId(&entry->invariant, table->dependencies[instr1].ID, instrs.instructions[instr1].dest);
                    }
        }
    }
    else
    {
        // if there is no loop all dependencies are local
        pushId(&entry->local, table->dependencies[instr1].ID, instrs.instructions[instr1].dest);
    }

    return;
}

dependencyTable fillDepencies(dependencyTable table)
{
    for (int i = 0; i < table.size; i++)
    {

        int pot = instrs.instructions[i].dest;

        for (int j = i; j < instrs.size; j++)
        {
            // check for each instruction if pot is a dest or src
            if (instrs.instructions[j].dest == pot)
            {
                break;
            }
            whatType(i, j, &table);
        }
        // if instruction is in block 1 we check for the instructions before in block 1
        // from start of loop to i
        if (instrs.instructions[i].block == 1 && instrs.loop_start != -1)
        {

            for (int j = instrs.loop_start; j < i; j++)
            {
                whatType(i, j, &table);
            }
        }
    }
    return table;
}

void showDepTable(dependencyTable table)
{
    for (int i = 0; i < table.size; i++)
    {
        printf("Instruction %d\n", table.dependencies[i].address);
        printf("Local\n");
        for (int j = 0; j < table.dependencies[i].local.size; j++)
        {
            printf("ID: %c, Reg: %d\n", table.dependencies[i].local.list[j].ID, table.dependencies[i].local.list[j].reg);
        }
        printf("Loop\n");
        for (int j = 0; j < table.dependencies[i].loop.size; j++)
        {
            printf("ID: %c, Reg: %d\n", table.dependencies[i].loop.list[j].ID, table.dependencies[i].loop.list[j].reg);
        }
        printf("Invariant\n");
        for (int j = 0; j < table.dependencies[i].invariant.size; j++)
        {
            printf("ID: %c, Reg: %d\n", table.dependencies[i].invariant.list[j].ID, table.dependencies[i].invariant.list[j].reg);
        }
        printf("Post Loop\n");
        for (int j = 0; j < table.dependencies[i].postL.size; j++)
        {
            printf("ID: %c, Reg: %d\n", table.dependencies[i].postL.list[j].ID, table.dependencies[i].postL.list[j].reg);
        }
    }
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