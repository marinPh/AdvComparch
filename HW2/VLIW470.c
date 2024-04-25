#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "lib/cJSON.h"
#include "lib/cJSON_Utils.h"
#include "lib/VLIW470.h"

#define REGS 96
#define ROTATION_START_INDEX 32 // Start index of rotating registers
#define LOOP_AROUND 64 // Number of registers in the rotating register file

InstructionsSet instrs = {NULL, 0};

// FUs
typedef InstructionEntry ALU; 
typedef InstructionEntry Mult;
typedef InstructionEntry Mem;
typedef InstructionEntry Br;

typedef struct {
    ALU  alu1;
    ALU  alu2;
    Mult mult; // 3 cycle latency (all others 1) 
    Mem  mem;
    Br   br; 
} VLIW;

typedef struct {
    VLIW *vliw;
    int size;
} VLIWBundles;

// Dictionary {instruction type: latency}
int latencies[10] = {1, 1, 1, 3, 1, 1, 1, 1, 1, 1};

typedef struct {
    unsigned int  PC; // Program Counter
    unsigned int  LC; // Loop Count
    unsigned int  EC; // Epilogue Count
    unsigned int  RRB; // Register Rotation Base
    unsigned long PhysRegFile[REGS]; // Physical Register File (96 registers, 64 bits each)
    unsigned long PredRegFile[REGS]; // Predicate Register File
    unsigned int  FUCount[FU]; // Number of each type of FU: [ALU, MULT, MEM, BR]
    VLIWBundles   bundles; // VLIW instruction bundles
    unsigned int  II; // Initiation Interval
} ProcessorState;

ProcessorState state;

/*
* Initialize the processor state.
*/
void initProcessorState() {
    state.PC = 0;
    state.LC = 0;
    state.EC = 0;
    state.RRB = 0;
    memset(state.PhysRegFile, 0, sizeof(state.PhysRegFile));
    memset(state.PredRegFile, 0, sizeof(state.PredRegFile));
    state.FUCount[0] = 2;
    state.FUCount[1] = 1;
    state.FUCount[2] = 1;
    state.FUCount[3] = 1;
    state.bundles.vliw = NULL;
    state.II = 1;
}

/**
 * Calculate the rotated register index for general-purpose or predicate registers.
 * @param baseIndex The original index of the register.
 * @param rrb The register rotation base.
 * @return The rotated register index.
 */
int getRotatedRegisterIndex(int baseIndex, int rrb) {
    if (baseIndex >= ROTATION_START_INDEX && baseIndex < REGS) {
        int rotatedIndex = baseIndex - rrb;
        // Wrap around if the calculated index goes past the range of rotating registers
        if (rotatedIndex < ROTATION_START_INDEX) {
            rotatedIndex += LOOP_AROUND;
        }
        return rotatedIndex;
    }
    // Return the original index if it's not within the rotating range
    return baseIndex;
}

/**
 * Access a general-purpose register value with rotation applied.
 * @param state Pointer to the processor state.
 * @param index Index of the register to access.
 * @return The value of the rotated register.
 */
int readGeneralRegister(ProcessorState *state, int index) {
    int rotatedIndex = getRotatedRegisterIndex(index, state->RRB);
    return state->PhysRegFile[rotatedIndex];
}

/**
 * Access a predicate register value with rotation applied.
 * @param state Pointer to the processor state.
 * @param index Index of the predicate register to access.
 * @return The status of the rotated predicate register.
 */
bool readPredicateRegister(ProcessorState *state, int index) {
    int rotatedIndex = getRotatedRegisterIndex(index, state->RRB);
    return state->PredRegFile[rotatedIndex];
}


/**
 * Lower bound the Initiation Interval (II), which is the number of issue cycles between the start of successive loop iterations.
 * @param set Set of instructions to calculate the II for.
 * @param state Pointer to the processor state containing the FUs available in the processor.
 * @return The lower bound of the II.
 */
int calculateIIRes(InstructionsSet *set, ProcessorState *state) {
    int counts[10] = {0};  // Array to count instruction types

    // Counting instructions by type
    for (int i = 0; i < set->size; i++) {
        if (set->instructions[i].type != NOP) { // Ignore NOPs    TODO set->instructions[i].type != ST  
            counts[set->instructions[i].type]++;
        }
    }

    // Calculation of II based on FU resources
    int II_res = 0;
    II_res = (counts[ADD] + counts[ADDI] + counts[SUB] + counts[MOV] + state->FUCount[0] - 1) / state->FUCount[0];
    II_res = fmax(II_res, (counts[MULU] + state->FUCount[1] - 1) / state->FUCount[1]);
    II_res = fmax(II_res, (counts[LD] + state->FUCount[2] - 1) / state->FUCount[2]);
    II_res = fmax(II_res, (counts[LOOP] + counts[LOOP_PIP] + state->FUCount[3] - 1) / state->FUCount[3]);

    return II_res;  // Want to minimize it to have more pipeline parallelism
}

// int checkInterloopDependencies(DependencyTable *table, ProcessorState *state) {
//     int maxII = state->II;
//     int changed = 0;

//     for (int i = 0; i < table->numInstructions; i++) {
//         Instruction *current = &table->instructions[i];
//         if (current->interloopDeps) {
//             for (int j = 0; j < table->numInstructions; j++) {
//                 if (table->instructions[j].id == current->interloopDeps) {
//                     Instruction *dependency = &table->instructions[j];
//                     // Check the interloop dependency condition
//                     if (dependency->scheduledTime + dependency->latency > current->scheduledTime + state->II) {
//                         int newII = dependency->scheduledTime + dependency->latency - current->scheduledTime;
//                         if (newII > maxII) {
//                             maxII = newII;
//                             changed = 1;
//                         }
//                     }
//                 }
//             }
//         }
//     }

//     if (changed) {
//         state->II = maxII;
//         return 1;  // II was changed
//     }
//     return 0;  // II was not changed
// }

/**
 * Check and adjust the Initiation Interval (II) for an instruction based on interloop dependencies of an intruction.
 * @param table Dependency table containing the instructions.
 * @param instrAddr Address of the instruction to check and adjust the II for.
 * @param state Processor state containing the current II.
 * @return int 1 if the II was changed, 0 otherwise.
 */
int checkAndAdjustIIForInstruction(DependencyTable *table, char instrAddr, ProcessorState *state) {
    int changed = 0;

    // Find the instruction in the dependency table
    DepTableEntry *current = table->instructions[instrAddr];  // TODO talk to Marin -> index by addr ? 

    // If the instruction has an interloop dependency
    if (current->interloopDeps != -1) {   // TODO talk to Marin -> set addr inside interloopDeps ?      but may conatin multiple
        DepTableEntry *dependency = table->instructions[current->interloopDeps];
        // Check the interloop dependency condition
        int latency = latencies[dependency->instr];
        if (dependency->scheduledTime + latency > current->scheduledTime + state->II) {   // TODO scheduled time & latency    with Marin Instr. contain enum type
            // Adjust the II to satisfy the interloop dependency
            state->II = dependency->scheduledTime + dependency->latency - current->scheduledTime;
            changed = 1;
            return 1; // II was changed
        }
    }
    return 0;  // II was not changed
}







//*************************************************************
//
//                     Pipeline Stages
//
//*************************************************************

// TODO when to update the DepTable?
// statically before fetching ? or during fetch ? 

/**
 * Fetch and Decode stage of the pipeline.
 * @param state Pointer to the processor state.
 */
void FetchAndDecode(ProcessorState *state) {
    // Fetch the instruction from the instruction set
    InstructionEntry instr = instrs.instructions[state->PC];
    // Decode the instruction by filling in the DependencyTable
    // and checking for interloop dependencies

    // TODO fill in the DependencyTable


    // Increment the Program Counter
    state->PC++;    
}

/**
 * Issue stage of the pipeline.
 * @param state Pointer to the processor state.
 */
void Issue(ProcessorState *state, DependencyTable *table) {         // TODO no loops yet
    // Take instruction in Program Order from DependencyTable
    // if no dependencies and FU available schedule it in the VLIW bundle
    // else wait for dependencies to be resolved

    // Create a new VLIW bundle
    state->bundles.size += 1;
    state->bundles.vliw = (VLIW*)realloc(state->bundles.vliw, state->bundles.size * sizeof(VLIW));
    state->bundles.vliw[state->bundles.size - 1] = {NOP, NOP, NOP, NOP, NOP};

    VLIW *vliw = &state->bundles.vliw[state->bundles.size - 1];

    for (int i = 0; i < table->size, i++) {
        DepTableEntry *entry = table->instructions[i];
        // If the instruction is not scheduled yet
        if (entry->scheduledTime == -1) {
            // If there are no local dependencies
            if (entry->localDeps == -1) {
                // Schedule the instruction in the VLIW bundle
                switch (entry->instr) {
                    case ADD:
                        if (vliw->alu1.type == NOP) {
                            vliw->alu1 = instrs.instructions[entry->instrAddr];
                        } else if (vliw->alu2.type == NOP) {
                            vliw->alu2 = instrs.instructions[entry->instrAddr];
                        }
                        break;
                    case ADDI:
                        if (vliw->alu1.type == NOP) {
                            vliw->alu1 = instrs.instructions[entry->instrAddr];
                        } else if (vliw->alu2.type == NOP) {
                            vliw->alu2 = instrs.instructions[entry->instrAddr];
                        }
                        break;
                    case SUB:
                        if (vliw->alu1.type == NOP) {
                            vliw->alu1 = instrs.instructions[entry->instrAddr];
                        } else if (vliw->alu2.type == NOP) {
                            vliw->alu2 = instrs.instructions[entry->instrAddr];
                        }
                        break;
                    case MOV:
                        if (vliw->alu1.type == NOP) {
                            vliw->alu1 = instrs.instructions[entry->instrAddr];
                        } else if (vliw->alu2.type == NOP) {
                            vliw->alu2 = instrs.instructions[entry->instrAddr];
                        }
                        break;
                    case MULU:
                        if (vliw->mult.type == NOP) {
                            vliw->mult = instrs.instructions[entry->instrAddr];
                        }
                        break;
                    case LD:
                        if (vliw->mem.type == NOP) {
                            vliw->mem = instrs.instructions[entry->instrAddr];
                        }
                        break;
                    case ST:  // TODO store operations are not considered
                        if (vliw->mem.type == NOP) {
                            vliw->mem = instrs.instructions[entry->instrAddr];
                        }
                        break;
                    case LOOP:
                        if (vliw->br.type == NOP) {
                            vliw->br = instrs.instructions[entry->instrAddr];
                        }
                        break;
                    case LOOP_PIP:
                        if (vliw->br.type == NOP) {
                            vliw->br = instrs.instructions[entry->instrAddr];
                        }
                        break;
                    default:
                        break;
                }
                // Schedule the instruction in the VLIW bundle
                // Update the scheduled time in the DependencyTable    TODO with Marin
                entry->scheduledTime = state->PC; // or PC ? state->bundles.size - 1 ?
            }
            else if (/* condition */)
            {
                /* code */
            }
            
        }
    }
}

/**
 * Execute stage of the pipeline.
 * @param state Pointer to the processor state.
 */
void Execute(ProcessorState *state) {
    // for the instruction in the VLIW bundle with scheduled time = PC, execute it
    // if done update the register file

    // ALU1
    state->bundles.vliw[state->PC].alu1.cycle = 1;
    state->bundles.vliw[state->PC].alu1.done  = true;  // even if NOP 
    if (state->bundles.vliw[state->PC].alu1.type == ADD || state->bundles.vliw[state->PC].alu1.type == ADDI) {
        state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src1] + state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src2];
    } else if (state->bundles.vliw[state->PC].alu1.type == SUB) {
        state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src1] - state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src2];
    } else if (state->bundles.vliw[state->PC].alu1.type == MOV) {
        state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src1];
    }

    // ALU2
    state->bundles.vliw[state->PC].alu2.cycle = 1;
    state->bundles.vliw[state->PC].alu2.done  = true; 
    if (state->bundles.vliw[state->PC].alu2.type == ADD || state->bundles.vliw[state->PC].alu2.type == ADDI) {
        state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src1] + state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src2];
    } else if (state->bundles.vliw[state->PC].alu2.type == SUB) {
        state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src1] - state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src2];
    } else if (state->bundles.vliw[state->PC].alu2.type == MOV) {
        state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src1];
    }

    // MULT
    if (state->bundles.vliw[state->PC - 1].mult.done == false && state->bundles.vliw[state->PC - 1].mult.type == MULU) {
        // if NOP and previous instruction was an UNFINISHED MULU, copy the mult instruction from the previous cycle
        state->bundles.vliw[state->PC].mult = state->bundles.vliw[state->PC - 1].mult;   // TODO by refernece .... but it's what we want 
    }
    if (state->bundles.vliw[state->PC].mult.type == MULU) {  // if MULU, start/keep executing it 
        state->bundles.vliw[state->PC].mult.cycle += 1;
        if (state->bundles.vliw[state->PC].mult.cycle == 3) {
            state->bundles.vliw[state->PC].mult.done = true;
            state->PhysRegFile[state->bundles.vliw[state->PC].mult.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].mult.src1] * state->PhysRegFile[state->bundles.vliw[state->PC].mult.src2];
        }
    } else { // if NOP, set done to true 
        // (this is the case where NO MULU instructions are present in the VLIW bundle
        // or the previous MULU instruction is done)
        state->bundles.vliw[state->PC].mult.done = true;
    }

    //********************        //********************
    //       MULU                 //    MULU cycle 1
    //       NOP             =>   //    MULU cycle 2
    //       NOP                  //    MULU cycle 3, done
    //       NOP                  //    NOP  done 
    //********************        //********************

    //********************        //********************
    //       NOP                  //    NOP  done
    //       NOP                  //    NOP  done
    //       MULU                 //    MULU cycle 1
    //       NOP             =>   //    MULU cycle 2
    //       NOP                  //    MULU cycle 3, done
    //       NOP                  //    NOP  done 
    //********************        //********************

    // LD 
    state->bundles.vliw[state->PC].mem.cycle = 1;
    state->bundles.vliw[state->PC].mem.done  = true;
    if (state->bundles.vliw[state->PC].mem.type == LD) {
        state->PhysRegFile[state->bundles.vliw[state->PC].mem.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].mem.src1] + state->bundles.vliw[state->PC].mem.address;
    } // TODO store operations are not considered ?? 

    // BR (LOOP, LOOP_PIP)
    state->bundles.vliw[state->PC].br.cycle = 1;
    state->bundles.vliw[state->PC].br.done  = true;
    if (state->bundles.vliw[state->PC].br.type == LOOP) {
        state->LC = state->bundles.vliw[state->PC].br.loopStart;
    } else if (state->bundles.vliw[state->PC].br.type == LOOP_PIP) {
        state->LC = state->bundles.vliw[state->PC].br.loopStart;
    }
}

/**
 * Commit stage of the pipeline.
 * @param state Pointer to the processor state.
 */
void Commit(ProcessorState *state) {
    // Commit the instructions in the VLIW bundle
    // if done update the register file
    // TODO how to update DepTable? 
    // TODO
}