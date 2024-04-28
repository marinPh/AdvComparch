#ifndef VLIW470_H
#define VLIW470_H

#include "utils.h"

#define REGS 96
#define BUNDLE 5 // Number of instructions executed in parallel
#define FU 4 // Number of functional unit types: ALU, MULT, MEM, BR

// Structure for parsing JSON
typedef struct {
    InstructionEntry *instructions;
    size_t size;
    unsigned int loopStart;  // Used for branching operations to store the loop start address
    unsigned int loopEnd;
} InstructionsSet;

typedef struct {
    char ID;
    unsigned int reg;
} dependency;

typedef struct {
    dependency *list;
    unsigned int size;
} idList;

typedef struct {
    unsigned int address;
    char ID;
    InstructionType type;
    unsigned int dest;
    idList local;
    idList loop;
    idList invariant;
    idList postL;
    int scheduledTime;
} DependencyEntry;

typedef struct {
    DependencyEntry *dependencies;
    unsigned int size;
} DependencyTable;

// FUs
typedef InstructionEntry ALU; 
typedef InstructionEntry Mult;
typedef InstructionEntry Mem;
typedef InstructionEntry Br;

typedef struct {
    ALU  *alu1;
    ALU  *alu2;
    Mult *mult; // 3 cycle latency (all others 1)
    Mem  *mem;
    Br   *br;
} VLIW;

typedef struct {
    VLIW *vliw;
    int size;
} VLIWBundles;

// Dictionary {instruction type: latency}

typedef struct {
    unsigned int  PC; // Program Counter
    unsigned int  LC; // Loop Count
    unsigned int  EC; // Epilogue Count
    unsigned int  RRB; // Register Rotation Base
    unsigned long PhysRegFile[REGS]; // Physical Register File (96 registers, 64 bits each)
    unsigned long PredRegFile[REGS]; // Predicate Register File
    unsigned int  FUCount[FU]; // Number of each type of FU: [ALU, MULT, MEM, BR]
    VLIWBundles   bundles; // VLIW instruction bundles
    unsigned int  II;      // Initiation Interval
    unsigned int  stage;   // Loop stage 
} ProcessorState;

void parseInstrunctions(char* inputFile);

void printInstructions(InstructionsSet instr);

void showDepTable(DependencyTable table);
DependencyTable createFillDepencies();
DependencyTable dependencyTableInit();

void initProcessorState(ProcessorState *state);

int getRotatedRegisterIndex(int baseIndex, int rrb);
int readGeneralRegister(ProcessorState *state, int index);
bool readPredicateRegister(ProcessorState *state, int index);

int calculateIIRes(ProcessorState *state);

int checkInterloopDependencies(DependencyTable *table, ProcessorState *state);
int checkAndAdjustIIForInstruction(DependencyTable *table, int i, ProcessorState *state);

void registerAllocation(ProcessorState *state, DependencyTable *table);
void registerAllocationPip(ProcessorState *state, DependencyTable *table);

VLIW newVLIW(ProcessorState *state);

typedef struct {
    int latestALU1;
    int latestALU2;
    int latestMult;
    int latestMem;
    int latestBr;
} SchedulerState;

void scheduleInstruction(ProcessorState *state, DependencyEntry *entry, SchedulerState *schedulerState);
void scheduleInstructions(ProcessorState *state, DependencyTable *table);
void scheduleInstructionsPiP(ProcessorState *state, DependencyTable *table);


#endif /* MIPS_SIMULATOR_H */