#ifndef VLIW470_H
#define VLIW470_H

#include <stdio.h>
#include "utils.h"

// Instructions


// Structure for parsing JSON
typedef struct {
    InstructionEntry *instructions;
    unsigned int size;
    unsigned int loop_start;
    unsigned int loop_end;
} Instruction;

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

void parseInstrunctions(char* progFile, char* inputFile);

void printInstructions(Instruction instr);

void showDepTable(dependencyTable table);
dependencyTable fillDepencies();


#endif /* MIPS_SIMULATOR_H */