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

void parseInstrunctions(char* progFile, char* inputFile);

#endif /* MIPS_SIMULATOR_H */