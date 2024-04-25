#ifndef VLIW470_H
#define VLIW470_H

#define OPCODE 5
#define BUNDLE 5 // Number of instructions executed in parallel
#define FU 4 // Number of functional unit types: ALU, MULT, MEM, BR

// Instruction types
// typedef enum {
//     TYPE_R,     // Register to register operations: add, sub, mulu, mov where dest and source are registers
//     TYPE_I,     // Immediate operations: addi, ld (load address immediate), mov with immediate
//     TYPE_S,     // Store type with immediate offset for memory address, NO need to consider them
//     TYPE_B,     // Branch operations: loop, loop.pip, can NOT be predicated (always executed)
//     TYPE_P,     // Predicate operations: mov with predicate setting
//     TYPE_NOP    // No operation
// } InstructionType;

typedef enum {
    ADD, ADDI, SUB, MULU, LD, ST, NOP, MOV, LOOP, LOOP_PIP  // NO need to consider store operations
} InstructionType;

// Instructions
typedef struct {
    char opcode[OPCODE]; // 4 characters + null terminator
    int dest;
    int src1;            // Used as source register or as an immediate value, depending on the instruction type
    int src2;            // Used only by register to register operations (e.g., add, sub, mulu)
    int imm;             // Immediate value for immediate operations (e.g., addi, ld, mov with immediate)
    bool predicate;      // For conditional execution; true or false for LOOP types, ignored otherwise => modifies processor state only if true, else discarded at commit
    InstructionType type;// Type of the instruction to handle different formats
    int loopStart;       // Used for branching operations to store the loop start address
    int cycle;           // Cycles the instruction has been in the execution stage
    bool done;           // True if the instruction has been executed
} InstructionEntry;

// Structure for parsing JSON
typedef struct {
    InstructionEntry *instructions;
    size_t size;
} InstructionsSet;


#endif /* MIPS_SIMULATOR_H */