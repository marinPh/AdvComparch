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

void registerAllocation(ProcessorState *state, DependencyTable *table) {  // this has TERRIBLE complexity (O(n^2)
    // Allocate registers to instructions in the VLIW bundle 
    // based on the DependencyTable
    // TODO

    int reg = 0;

    for (int i = 0; i < state->bundles.size; i++) {
        VLIW *vliw = &state->bundles.vliw[i];
        // ALU1
        if (vliw->alu1.type != NOP) {
            // // Check if the instruction is not scheduled yet
            // if (table->instructions[vliw->alu1.instrAddr].scheduledTime == -1) {
                // Allocate registers
                table->instructions[vliw->alu1.instrAddr].dest = reg;  // TODO check if it's the right way to do it
                vliw->alu1.dest = reg;  // TODO
                reg++;
            // }
        }
        // ALU2
        if (vliw->alu2.type != NOP) {
            // // Check if the instruction is not scheduled yet
            // if (table->instructions[vliw->alu2.instrAddr].scheduledTime == -1) {
                // Allocate registers
                table->instructions[vliw->alu2.instrAddr].dest = reg;  // TODO check if it's the right way to do it
                vliw->alu2.dest = reg;  // TODO
                reg++;
            // }
        }
        // MULT
        if (vliw->mult.type != NOP) {
            // // Check if the instruction is not scheduled yet
            // if (table->instructions[vliw->mult.instrAddr].scheduledTime == -1) {
                // Allocate registers
                table->instructions[vliw->mult.instrAddr].dest = reg;  // TODO check if it's the right way to do it
                vliw->mult.dest = reg;  // TODO
                reg++;
            // }
        }
        // MEM
        if (vliw->mem.type != NOP) {
            // // Check if the instruction is not scheduled yet
            // if (table->instructions[vliw->mem.instrAddr].scheduledTime == -1) {
                // Allocate registers
                table->instructions[vliw->mem.instrAddr].dest = reg;  // TODO check if it's the right way to do it
                vliw->mem.dest = reg;  // TODO
                reg++;
            // }
        }
        // BR
        if (vliw->br.type != NOP) {
            // TODO 
        }

        // Iterate over the instructions in the DependencyTable and if they have dependencies 
        // in the localDeps, interloopDeps, invariantDeps or postloopDeps, 
        // change the source registers to the allocated destination registers of the instructions they depend on
        for (int j = 0; j < table->size; j++) {
            DepTableEntry *entry = table->instructions[j];
            // localDeps
            if (entry->localDeps == vliw->alu1.instrAddr) {
                entry->localDeps = vliw->alu1.dest;
            }
            if (entry->localDeps == vliw->alu2.instrAddr) {
                entry->localDeps = vliw->alu2.dest;
            }
            if (entry->localDeps == vliw->mult.instrAddr) {
                entry->localDeps = vliw->mult.dest;
            }
            if (entry->localDeps == vliw->mem.instrAddr) {
                entry->localDeps = vliw->mem.dest;
            }
            // interloopDeps
            if (entry->interloopDeps == vliw->alu1.instrAddr) {
                entry->interloopDeps = vliw->alu1.dest;
            }
            if (entry->interloopDeps == vliw->alu2.instrAddr) {
                entry->interloopDeps = vliw->alu2.dest;
            }
            if (entry->interloopDeps == vliw->mult.instrAddr) {
                entry->interloopDeps = vliw->mult.dest;
            }
            if (entry->interloopDeps == vliw->mem.instrAddr) {
                entry->interloopDeps = vliw->mem.dest;
            }
            // invariantDeps
            if (entry->invariantDeps == vliw->alu1.instrAddr) {
                entry->invariantDeps = vliw->alu1.dest;
            }
            if (entry->invariantDeps == vliw->alu2.instrAddr) {
                entry->invariantDeps = vliw->alu2.dest;
            }
            if (entry->invariantDeps == vliw->mult.instrAddr) {
                entry->invariantDeps = vliw->mult.dest;
            }
            if (entry->invariantDeps == vliw->mem.instrAddr) {
                entry->invariantDeps = vliw->mem.dest;
            }
            // postloopDeps
            if (entry->postloopDeps == vliw->alu1.instrAddr) {
                entry->postloopDeps = vliw->alu1.dest;
            }
            if (entry->postloopDeps == vliw->alu2.instrAddr) {
                entry->postloopDeps = vliw->alu2.dest;
            }
            if (entry->postloopDeps == vliw->mult.instrAddr) {
                entry->postloopDeps = vliw->mult.dest;
            }
            if (entry->postloopDeps == vliw->mem.instrAddr) {
                entry->postloopDeps = vliw->mem.dest;
            }
        }
    }

    // fix interloopDeps
    // for every instruction in DependencyTable, check if it has an interloop dependency
    // if yes, schedule a MOV instruction to copy the value of the source register to the destination register
    for (int j = 0; j < table->size; j++) {  // TODO repetitive.... 
        DepTableEntry *entry = table->instructions[j];
        if (entry->interloopDeps != -1) {
            // save the intruction it has a dependency on
            // look through the instructions in the dependency table to find the one with the latest schedule time among all those with the same interloopDeps
            int latestScheduledTime = entry->scheduledTime;
            int latestScheduledInstrAddr = entry->instrAddr;
            for (int k = 0; k < table->size; k++) {
                DepTableEntry *entry2 = table->instructions[k];
                if (entry2->interloopDeps == entry->interloopDeps) {
                    if (entry2->scheduledTime > latestScheduledTime) {
                        latestScheduledTime = entry2->scheduledTime;
                        latestScheduledInstrAddr = entry2->instrAddr;
                    }
                }
            }

            // schedule a MOV instruction to copy the value of the source register to the destination register
            // in the VLIW bundle with the latest scheduled instruction if one of the two ALUs is free
            VLIW *vliw = &state->bundles.vliw[latestScheduledInstrAddr];
            if (vliw->alu1.type == NOP) {
                vliw->alu1.type = MOV;
                vliw->alu1.src1 = table->instructions[entry->interloopDeps].dest;
                vliw->alu1.dest = entry->dest;   // TODO check if it's the right way to do it
                // Set the other fields of the MOV instruction
                vliw->alu1.imm = -1;
                vliw->alu1.predicate = false;
                vliw->alu1.loopStart = -1;
                vliw->alu1.cycle = 0;
                vliw->alu1.done = false;
            } else if (vliw->alu2.type == NOP) {
                vliw->alu2.type = MOV;
                vliw->alu2.src1 = table->instructions[entry->interloopDeps].dest;
                vliw->alu2.dest = entry->dest;
                // Set the other fields of the MOV instruction
                vliw->alu1.imm = -1;
                vliw->alu1.predicate = false;
                vliw->alu1.loopStart = -1;
                vliw->alu1.cycle = 0;
                vliw->alu1.done = false;
            } else {
                // create a new VLIW bundle
                state->bundles.size += 1;
                state->bundles.vliw = (VLIW*)realloc(state->bundles.vliw, state->bundles.size * sizeof(VLIW));
                state->bundles.vliw[state->bundles.size - 1] = {NOP, NOP, NOP, NOP, NOP};

                VLIW *vliw2 = &state->bundles.vliw[state->bundles.size - 1];
                vliw2->alu1.type = MOV;
                vliw2->alu1.src1 = table->instructions[entry->interloopDeps].dest;
                vliw2->alu1.dest = entry->dest;
                // Set the other fields of the MOV instruction
                vliw2->alu1.imm = -1;
                vliw2->alu1.predicate = false;
                vliw2->alu1.loopStart = -1;
                vliw2->alu1.cycle = 0;

                // move the loop instruction to the new VLIW bundle
                vliw2->br = vliw->br;
                vliw->br.type = NOP;
                vliw->br.done = false; 
            }

        }
    }

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

    // Check and adjust the Initiation Interval (II) for the instruction
    // based on interloop dependencies
    // checkInterloopDependencies(table, state);  // TODO no loops yet

    // Perform register allocation
    ...
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
    switch (state->bundles.vliw[state->PC].alu1.type) {
        case ADD:
            state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src1] + state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src2];
            break;
        case ADDI:
            state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src1] + state->bundles.vliw[state->PC].alu1.imm;
            break;
        case SUB:
            state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src1] - state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src2];
            break;
        case MOV:
            if (strcmp(state->bundles.vliw[state->PC].alu1.dest, "LC") == 0) {  // case of mov LC
                state->LC = state->bundles.vliw[state->PC].alu1.src1;
                // state->EC = #loop stages i.e. #loop iterations
                state->Ec = state->bundles.vliw[state->PC].alu2.src1; // TODO check if it's the right way to do it
            } else if (state->bundles.vliw[state->PC].alu1.imm != -1) {  // case of mov with immediate
                state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->bundles.vliw[state->PC].alu1.imm;
            } else {  // case of mov with register
                state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src1];
            }
            break;
        default:
            break;
    }


    // ALU2
    state->bundles.vliw[state->PC].alu2.cycle = 1;
    state->bundles.vliw[state->PC].alu2.done  = true; 
    switch (state->bundles.vliw[state->PC].alu2.type) {
        case ADD:
            state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src1] + state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src2];
            break;
        case ADDI:
            state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src1] + state->bundles.vliw[state->PC].alu2.imm;
            break;
        case SUB:
            state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src1] - state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src2];
            break;
        case MOV:
            if (strcmp(state->bundles.vliw[state->PC].alu2.dest, "LC") == 0) {  // case of mov LC
                state->LC = state->bundles.vliw[state->PC].alu2.src1;
                // state->EC = #loop stages i.e. #loop iterations
                state->Ec = state->bundles.vliw[state->PC].alu2.src1; // TODO check if it's the right way to do it 
            } else if (state->bundles.vliw[state->PC].alu2.imm != -1) {  // case of mov with immediate
                state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->bundles.vliw[state->PC].alu2.imm;
            } else {  // case of mov with register
                state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src1];
            }
            break;
        default:
            break;
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
        state->PhysRegFile[state->bundles.vliw[state->PC].mem.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].mem.src1] + state->bundles.vliw[state->PC].mem.imm;
    } // TODO store operations are not considered ?? 

    // BR (LOOP, LOOP_PIP)
    state->bundles.vliw[state->PC].br.cycle = 1;
    state->bundles.vliw[state->PC].br.done  = true;
    if (state->bundles.vliw[state->PC].br.type == LOOP) {  // unconditional branch
        state->PC = state->bundles.vliw[state->PC].br.loopStart;   
    } else if (state->bundles.vliw[state->PC].br.type == LOOP_PIP) {  // conditional branch
        if (state->LC > 0) {
            state->LC -= 1;
            // state->EC unchanged
            state->RRB += 1;
            // enable stage predicate register  TODO ????? always p32 ? 
            state->PredRegFile[31] = true;
            state->PC = state->bundles.vliw[state->PC].br.loopStart;
        } else {
            if (state->EC > 0) {
                // state->LC unchanged
                state->EC -= 1;
                state->RRB += 1;
                // disable stage predicate register  TODO ????? 
                state->PredRegFile[31] = false;
                state->PC = state->bundles.vliw[state->PC].br.loopStart;
            } else {
                //state->PC += 1;  // TODO nothing ? bc was already incremented by FetchAndDecode ? 
            }
        }
    }
}

/**
 * Commit stage of the pipeline.
 * @param state Pointer to the processor state.
 */
void Commit(ProcessorState *state, DependencyTable *table) {
    // Commit the instructions in the VLIW bundle
    // if done update the register file
    // TODO how to update DepTable? 
    // TODO
    if (state->bundles.vliw[state->PC].alu1.done) {
        state->bundles.vliw[state->PC].alu1.done = false;
    }
    if (state->bundles.vliw[state->PC].alu2.done) {
        state->bundles.vliw[state->PC].alu2.done = false;
    }
    if (state->bundles.vliw[state->PC].mult.done) {
        state->bundles.vliw[state->PC].mult.done = false;
    }
    if (state->bundles.vliw[state->PC].mem.done) {
        state->bundles.vliw[state->PC].mem.done = false;
    }
    if (state->bundles.vliw[state->PC].br.done) {
        state->bundles.vliw[state->PC].br.done = false;
    }
}