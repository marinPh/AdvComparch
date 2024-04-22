#ifndef VLIW470_H
#define VLIW470_H

#define OPCODE 5

// Instructions
typedef struct {
    char opcode[OPCODE]; // 4 characters + null terminator
    unsigned int block;
    int dest;
    int src1;
    int src2;
} InstructionEntry;

// Structure for parsing JSON
typedef struct {
    InstructionEntry *instructions;
    unsigned int size;
    unsigned int loop_start;
    unsigned int loop_end;
} Instruction;

#endif /* MIPS_SIMULATOR_H */