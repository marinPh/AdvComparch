#ifndef VLIW470_H
#define VLIW470_H

#define OPCODE 5

// Instructions
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

#endif /* MIPS_SIMULATOR_H */