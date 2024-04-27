#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "lib/cJSON.h"
#include "lib/cJSON_Utils.h"
#include "lib/VLIW470.h"
#include "lib/utils.h"

#define ROTATION_START_INDEX 32 // Start index of rotating registers
#define LOOP_AROUND 64 // Number of registers in the rotating register file

InstructionsSet instrs   = {NULL, 0};
DependencyTable depTable = {NULL, 0};
ProcessorState state;

InstructionEntry NOPinstr = {"nop", 0, -1, -1, -1, -1, NOP, 0, false};

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
 * @brief Add a dependency entry to the dependency table.
 * @param table The pointer to the dependency table.
 * @param entry The dependency entry to add.
 */
void pushDependency(DependencyTable *table, DependencyEntry entry) {
    table->dependencies = realloc(table->dependencies, (table->size + 1) * sizeof(DependencyEntry));
    table->dependencies[table->size] = entry;
    table->size++;
}

/**
 * @brief Initialize the dependency table with the instructions in the instruction set.
 * @return DependencyTable 
 */
DependencyTable dependencyTableInit() {
    DependencyTable table = {NULL, 0};
    for (int i = 0; i < instrs.size; i++) {
        DependencyEntry entry = {i, 65+i, instrs.instructions[i].type, instrs.instructions[i].dest, {0, {NULL, 0}}, {0, {NULL, 0}}, {0, {NULL, 0}}, NULL, -1};  // TODO postLoop only 1?
        pushDependency(&table, entry);
    }
    return table;
}

/**
 * @brief Fill the dependency table with dependencies between instructions.
 * 
 * @return DependencyTable 
 */
DependencyTable fillDepencies() {
    DependencyTable table = dependencyTableInit();
    for (int i = 0; i < table.size; i++) {
        int pot = instrs.instructions[i].dest;
        for (int j = i+1; j < instrs.size; j++) {
            // check for each instruction if pot is a dest or src
            if (instrs.instructions[j].dest == pot) break; 
            whatType(i, j, &table);
        }
        // if instruction is in block 1 we check for the instructions before in block 1
        // from start of loop to i
        if (instrs.instructions[i].block == 1 && instrs.loopStart != -1) {
            for (int j = instrs.loopStart; j <= i; j++) {
                whatType(i, j, &table);
            }
        }
    }
    return table;
}

/**
 * @brief Check the dependencies between two instructions.
 * @param instr1 The index of the first instruction.
 * @param instr2 The index of the second instruction.
 * @param table The dependency table pointer.
 */
void whatType(int instr1, int instr2, DependencyTable *table) {
    DependencyEntry *entry = &(table->dependencies[instr2]);
    // we check if the dest of instr1 is equal to src1 or src2 of instr2
    if (instrs.instructions[instr1].dest == instrs.instructions[instr2].src1 || instrs.instructions[instr2].src2 == instrs.instructions[instr1].dest) {
        if (instrs.loopStart != -1)
        { // check if in same block
            if (instrs.instructions[instr1].block == instrs.instructions[instr2].block) {
                // check if instr1 > instr2
                if (instr1 > instr2) {
                    pushId(&entry->loop, table->dependencies[instr1].ID, instrs.instructions[instr1].dest);
                } else {
                    pushId(&entry->local, table->dependencies[instr1].ID, instrs.instructions[instr1].dest);
                }
            } else {
                // check if in post loop means instr1 is in block 1 and instr2 is in block 2
                if (instrs.instructions[instr1].block == 1 && instrs.instructions[instr2].block == 2) {
                    pushId(&entry->postL, table->dependencies[instr1].ID, instrs.instructions[instr1].dest);
                } else {
                    // check if in invariant
                    if (instrs.instructions[instr1].block == 0 && (instrs.instructions[instr2].block == 1 || instrs.instructions[instr2].block == 2)) {
                        pushId(&entry->invariant, table->dependencies[instr1].ID, instrs.instructions[instr1].dest);
                    }
                }
            }
        // TODO } issue x Marin

        } else { // TODO:Waring very false
            // if there is no loop all dependencies are local
            pushId(&entry->local, table->dependencies[instr1].ID, instrs.instructions[instr1].dest);
        }
    }
    return;
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

/**
 * @brief Check and adjust the Initiation Interval (II) to satisfy interloop dependencies.
 * @param table Dependency table containing the instructions.
 * @param state Processor state containing the current II.
 * @return int 1 if the II was changed, 0 otherwise.
 */
int checkInterloopDependencies(DependencyTable *table, ProcessorState *state) {
    int changed = 0;

    // checkAndAdjustIIForInstruction for every instruction in BB1
    for (int i = instrs.loopStart; i < instrs.loopEnd+1; i++) {
        changed = checkAndAdjustIIForInstruction(table, i, state);
    }
    return changed;
}

/**
 * Check and adjust the Initiation Interval (II) for an instruction based on interloop dependencies of an intruction.
 * @param table Dependency table containing the instructions.
 * @param i Index in table of the instruction to check and adjust the II for.
 * @param state Processor state containing the current II.
 * @return int 1 if the II was changed, 0 otherwise.
 */
int checkAndAdjustIIForInstruction(DependencyTable *table, int i, ProcessorState *state) {
    int changed = 0;

    // Find the instruction in the dependency table
    DependencyEntry *current = table->dependencies[i];   

    // If the instruction has an interloop dependency
    for (int i = 0; i < current->loop.size; i++) {   
        DependencyEntry *dependency = current->loop.list[i];
        // Check the interloop dependency condition
        int latency = latencies[dependency->type];
        if (dependency->scheduledTime + latency > current->scheduledTime + state->II) {   
            // Adjust the II to satisfy the interloop dependency
            state->II = dependency->scheduledTime + dependency->latency - current->scheduledTime;
            changed = 1;
        }
    }
    return changed;  // Return 1 if the II was changed, 0 otherwise
}

// enum of dependency types
typedef enum {
    LOCAL, INTERLOOP, INVARIANT, POSTLOOP
} DepType;

typedef struct {
    int idx; 
    int idxOtherSrc;
    int dest;
    int destOtherSrc;
    int scheduledTime;
    int scheduledTimeOtherSrc; 
    DepType depType;
    DepType depTypeOtherSrc;
    int block; 
    int blockOtherSrc;
} LatestDependency;

/**
 * @brief Find the latest scheduled instruction that the entry instruction has a dependency on. NO interloop dependencies.
 * @param table The pointer to the dependency table.
 * @param entry The pointer to the dependency entry.
 * @return int The (ID-65) of the latest scheduled instruction that the entry instruction has a dependency on.
 */
LatestDependency findLatestDependency(DependencyTable *table, DependencyEntry *entry) {
    LatestDependency latestDep = {-1, -1, -1, LOCAL, -1};

    int latestScheduledTime = -1; // latest scheduled time of the instruction it has a dependency on
    int IDdependsOn = -1;

    int latestScheduledTimeOtherSrc = -1; // latest scheduled time of the instruction it has a dependency on
    int IDdependsOnOtherSrc = -1;

    int reg1 = instrs.instructions[entry->ID-65].src1; // source register 1
    int reg2 = instrs.instructions[entry->ID-65].src2; // source register 2

    // if it depends on a MULU and on another type of instruction that is scheduled later,
    // checking only the scheduled time may not be enough (see following case)
    // => need to check the scheduled time + 2 for MULU 
    // (2 because at the next cycle the result is available, and it allows to do a +1 for every type during scheduling)

    // EXAMPLE (using LOOP here, but valid for any dependency)

    // BB1 :
    // ADDI  with dependency on MUL and last ST
    // ST
    // MUL
    // ST  
    // LOOP

    //****************************************
    //     ALU1   ALU2   MULU   MEM   BR
    //
    // 0   ADDI   NOP    MUL    ST    NOP
    // 1   NOP    NOP    NOP    ST    LOOP     ADDI->ST resolved (1 cycle), but ADDI->MUL NOT resolved yet (2 cycles < 3)
    //                                                                          => need to add 1 NOP cycle and postpone the LOOP
    //****************************************

    // localDeps
    // find the latest scheduled intruction that it has a local dependency on
    for (int k = 0; k < entry->local.size; k++) {
        if (entry->local.list[j].reg == reg1) {
            if (table->dependencies[entry->local.list[j].ID-65].type == MULU) {
                if ((table->dependencies[entry->local.list[j].ID-65].scheduledTime + 2) > latestScheduledTime) {
                    latestScheduledTime = table->dependencies[entry->local.list[j].ID-65].scheduledTime + 2;
                    IDdependsOn = entry->local.list[j].ID-65;
                }
            } else if (table->dependencies[entry->local.list[j].ID-65].scheduledTime > latestScheduledTime) {
                latestScheduledTime = table->dependencies[entry->local.list[j].ID-65].scheduledTime;
                IDdependsOn = entry->local.list[j].ID-65;
            }
            latestDep.depType = LOCAL;
        }
        if (entry->local.list[j].reg == reg2) {
            if (table->dependencies[entry->local.list[j].ID-65].type == MULU) {
                if ((table->dependencies[entry->local.list[j].ID-65].scheduledTime + 2) > latestScheduledTimeOtherSrc) {
                    latestScheduledTimeOtherSrc = table->dependencies[entry->local.list[j].ID-65].scheduledTime + 2;
                    IDdependsOnOtherSrc = entry->local.list[j].ID-65;
                }
            } else if (table->dependencies[entry->local.list[j].ID-65].scheduledTime > latestScheduledTimeOtherSrc) {
                latestScheduledTimeOtherSrc = table->dependencies[entry->local.list[j].ID-65].scheduledTime;
                IDdependsOnOtherSrc = entry->local.list[j].ID-65;
            }
            latestDep.depTypeOtherSrc = LOCAL;
        }
    }
    // invariantDeps
    for (int k = 0; k < entry->invariant.size; k++) {
        if (entry->invariant.list[j].reg == reg1) {
            if (table->dependencies[entry->invariant.list[j].ID-65].type == MULU) {
                if ((table->dependencies[entry->invariant.list[j].ID-65].scheduledTime + 2) > latestScheduledTime) {
                    latestScheduledTime = table->dependencies[entry->invariant.list[j].ID-65].scheduledTime + 2;
                    IDdependsOn = entry->invariant.list[j].ID-65;
                }
            } else if (table->dependencies[entry->invariant.list[j].ID-65].scheduledTime > latestScheduledTime) {
                latestScheduledTime = table->dependencies[entry->invariant.list[j].ID-65].scheduledTime;
                IDdependsOn = entry->invariant.list[j].ID-65;
            }
            latestDep.depType = INVARIANT;
            // latestDep.block = instrs.instructions[IDdependsOn].block;
        }
        if (entry->invariant.list[j].reg == reg2) {
            if (table->dependencies[entry->invariant.list[j].ID-65].type == MULU) {
                if ((table->dependencies[entry->invariant.list[j].ID-65].scheduledTime + 2) > latestScheduledTimeOtherSrc) {
                    latestScheduledTimeOtherSrc = table->dependencies[entry->invariant.list[j].ID-65].scheduledTime + 2;
                    IDdependsOnOtherSrc = entry->invariant.list[j].ID-65;
                }
            } else if (table->dependencies[entry->invariant.list[j].ID-65].scheduledTime > latestScheduledTimeOtherSrc) {
                latestScheduledTimeOtherSrc = table->dependencies[entry->invariant.list[j].ID-65].scheduledTime;
                IDdependsOnOtherSrc = entry->invariant.list[j].ID-65;
            }
            latestDep.depTypeOtherSrc = INVARIANT;
            // latestDep.blockOtherSrc = instrs.instructions[IDdependsOnOtherSrc].block;
        }
    }
    // postloopDeps
    for (int k = 0; k < entry->postL.size; k++) {  // only if in BB2 
        // // case where entry depends on both BB0 and BB1 => set BB1 dependency       // TODO 
        // if (latestDep.depType == INVARIANT && latestDep.block == 0) {   // not finished but I don't think it's necessary
        //     continue;
        // }
        if (entry->postL.list[j].reg == reg1) {
            if (table->dependencies[entry->postL.list[j].ID-65].type == MULU) {
                if ((table->dependencies[entry->postL.list[j].ID-65].scheduledTime + 2) > latestScheduledTime) {
                    latestScheduledTime = table->dependencies[entry->postL.list[j].ID-65].scheduledTime + 2;
                    IDdependsOn = entry->postL.list[j].ID-65;
                }
            } else if (table->dependencies[entry->postL.list[j].ID-65].scheduledTime > latestScheduledTime) {
                latestScheduledTime = table->dependencies[entry->postL.list[j].ID-65].scheduledTime;
                IDdependsOn = entry->postL.list[j].ID-65;
            }
            latestDep.depType = POSTLOOP;
        }
        if (entry->postL.list[j].reg == reg2) {
            if (table->dependencies[entry->postL.list[j].ID-65].type == MULU) {
                if ((table->dependencies[entry->postL.list[j].ID-65].scheduledTime + 2) > latestScheduledTimeOtherSrc) {
                    latestScheduledTimeOtherSrc = table->dependencies[entry->postL.list[j].ID-65].scheduledTime + 2;
                    IDdependsOnOtherSrc = entry->postL.list[j].ID-65;
                }
            } else if (table->dependencies[entry->postL.list[j].ID-65].scheduledTime > latestScheduledTimeOtherSrc) {
                latestScheduledTimeOtherSrc = table->dependencies[entry->postL.list[j].ID-65].scheduledTime;
                IDdependsOnOtherSrc = entry->postL.list[j].ID-65;
            }
            latestDep.depTypeOtherSrc = POSTLOOP;
        }
    }

    // switch the dependencies found if latestScheduledTimeOtherSrc > latestScheduledTime
    // latestScheduledTime must always be the GREATEST scheduled time
    if (latestScheduledTimeOtherSrc > latestScheduledTime) {
        int temp = latestScheduledTime;
        latestScheduledTime = latestScheduledTimeOtherSrc;
        latestScheduledTimeOtherSrc = temp;
        temp = IDdependsOn;
        IDdependsOn = IDdependsOnOtherSrc;
        IDdependsOnOtherSrc = temp;
        DepType tempDep = latestDep.depType;
        latestDep.depType = latestDep.depTypeOtherSrc;
        latestDep.depTypeOtherSrc = tempDep;
    }

    latestDep.idx = IDdependsOn+65;
    latestDep.dest = table->dependencies[IDdependsOn].dest;
    latestDep.scheduledTime = latestScheduledTime;
    latestDep.block = instrs.instructions[IDdependsOn].block;

    latestDep.idxOtherSrc = IDdependsOnOtherSrc+65;
    latestDep.destOtherSrc = table->dependencies[IDdependsOnOtherSrc].dest;
    latestDep.scheduledTimeOtherSrc = latestScheduledTimeOtherSrc;
    latestDep.blockOtherSrc = instrs.instructions[IDdependsOnOtherSrc].block;

    return latestDep;
}

/**
 * @brief Rename the scheduled instructions to eliminate WARs and WAWs.
 * @param state The pointer to the processor state.
 * @param table The pointer to the dependency table.
 */
void registerAllocation(ProcessorState *state, DependencyTable *table) {  
    // Allocate registers to instructions in the VLIW bundle based on the DependencyTable

    int reg = 1;

    // Step 1. allocate registers to the destination registers of all instructions in the VLIW bundles
    for (int i = 0; i < state->bundles.size; i++) {
        VLIW *vliw = &state->bundles.vliw[i];
        // ALU1
        if (vliw->alu1.type != NOP) {
            // Allocate registers
            vliw->alu1.dest = reg;  
            reg++;
        }
        // ALU2
        if (vliw->alu2.type != NOP) {
            vliw->alu2.dest = reg;  
            reg++;
        }
        // MULT
        if (vliw->mult.type != NOP) {
            vliw->mult.dest = reg;  
            reg++;
        }
        // // MEM      TODO no dest ? 
        // if (vliw->mem.type != NOP) {
        //     vliw->mem.dest = reg; 
        //     reg++;
        // }
        // BR
        // if (vliw->br.type != NOP) {
        //     // TODO 
        // }
    }

    // Step 2. change the source registers to the allocated destination registers of the instructions they depend on
    // Iterate over the instructions in the DependencyTable and if they have dependencies 
    // in the localDeps, interloopDeps, invariantDeps or postloopDeps, 
    // change the source registers to the allocated destination registers of the instructions they depend on
    for (int j = 0; j < table->size; j++) {
        DependencyEntry *entry = table->dependencies[j];

        int reg1 = instrs.instructions[entry->ID-65].src1; // source register 1
        int reg2 = instrs.instructions[entry->ID-65].src2; // source register 2

        LatestDependency latestDep = findLatestDependency(table, entry);

        // interloopDeps
        // only check for interloop dependencies if the latest found dependency producer is NOT in BB0
        // (case where there exist 2 producers, one in BB0 and one in BB1, and the consumer is in BB1 or BB2)    TODO consumer in BB2 ? 
        if (!(latestDep.block == 0 && latestDep.dest == reg1)) {  // check interloop for reg1

            for (int k = 0; k < entry->interloop.size; k++) {
                if (entry->interloop.list[j].reg == reg1) {
                    if (table->dependencies[entry->interloop.list[j].ID-65].type == MULU) {
                        if ((table->dependencies[entry->interloop.list[j].ID-65].scheduledTime + 2) > latestDep.scheduledTime) {
                            latestDep.scheduledTime = table->dependencies[entry->interloop.list[j].ID-65].scheduledTime + 2;
                            latestDep.idx = entry->interloop.list[j].ID-65;
                        }
                    } else if (table->dependencies[entry->interloop.list[j].ID-65].scheduledTime > latestDep.scheduledTime) {
                        latestDep.scheduledTime = table->dependencies[entry->interloop.list[j].ID-65].scheduledTime;
                        latestDep.idx = entry->interloop.list[j].ID-65;
                    }
                }
            }
        }
        if (!(latestDep.blockOtherSrc == 0 && latestDep.destOtherSrc == reg2)) {  // check interloop for reg1

            for (int k = 0; k < entry->interloop.size; k++) {
                if (entry->interloop.list[j].reg == reg2) {
                    if (table->dependencies[entry->interloop.list[j].ID-65].type == MULU) {
                        if ((table->dependencies[entry->interloop.list[j].ID-65].scheduledTime + 2) > latestDep.scheduledTimeOtherSrc) {
                            latestDep.scheduledTimeOtherSrc = table->dependencies[entry->interloop.list[j].ID-65].scheduledTime + 2;
                            latestDep.idxOtherSrc = entry->interloop.list[j].ID-65;
                        }
                    } else if (table->dependencies[entry->interloop.list[j].ID-65].scheduledTime > latestDep.scheduledTimeOtherSrc) {
                        latestDep.scheduledTimeOtherSrc = table->dependencies[entry->interloop.list[j].ID-65].scheduledTime;
                        latestDep.idxOtherSrc = entry->interloop.list[j].ID-65;
                    }
                }
            }
        }


        // TODO               !!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        // change the source registers to the allocated destination registers of the instructions they depend on
        if (latestDep.idx != -1) {            
            instrs.instructions[entry->ID-65].src1 = instrs.instructions[latestDep.idx].dest;
        }
        if (latestDep.idxOtherSrc != -1) {
            instrs.instructions[entry->ID-65].src2 = instrs.instructions[latestDep.idxOtherSrc].dest;
        }
    }

    // Step 3. fix interloopDeps
    // for every instruction in DependencyTable, check if it has an interloop dependency with itself as a producer
    // if yes, schedule a MOV instruction to copy the value of the source register to the destination register
    for (int j = 0; j < table->size; j++) {  
        DependencyEntry *entry = table->dependencies[j];
        if (entry->interloopDeps != -1) {
            // check if it has a dependency with itself as a producer
            for (int k = 0; k < entry->loop.size; k++) {
                if (entry->loop.list[k].ID == entry->ID) {
                    // find the last time that the same register was used as a source register
                    int lastUsedTime = -1;
                    int idxLastUsed = -1;
                    int regDep = -1; 
                    for (int l = instrs.loopStart; l < instrs.loopEnd + 1; l++) {
                        if (instrs.instructions[l].src1 == instrs.instructions[entry->ID-65].dest || instrs.instructions[l].src2 == instrs.instructions[entry->ID-65].dest) {
                            if (table->dependencies[l].scheduledTime > lastUsedTime) {
                                lastUsedTime = table->dependencies[l].scheduledTime;
                                idxLastUsed = l;
                                regDep = instrs.instructions[l].src1 == instrs.instructions[entry->ID-65].dest ? instrs.instructions[l].src1 : instrs.instructions[l].src2;
                            }
                        }
                    }

                    // schedule a MOV instruction to copy the value of the source register to the destination register
                    // in the VLIW bundle with the latest scheduled instruction if one of the two ALUs is free
                    VLIW *vliw = &state->bundles.vliw[lastUsedTime];
                    if (vliw->alu1.type == NOP) {
                        vliw->alu1.type = MOV;
                        vliw->alu1.src1 = instrs.instructions[entry->ID-65].dest;
                        vliw->alu1.dest = regDep;   // TODO check if it's the right way to do it
                        // Set the other fields of the MOV instruction
                        vliw->alu1.imm = 0;
                        vliw->alu1.predicate = 0;
                        vliw->alu1.loopStart = -1;
                        vliw->alu1.cycle = 0;
                        vliw->alu1.done = false;
                    } else if (vliw->alu2.type == NOP) {
                        vliw->alu2.type = MOV;
                        vliw->alu2.src1 = instrs.instructions[entry->ID-65].dest;
                        vliw->alu2.dest = regDep;
                        // Set the other fields of the MOV instruction
                        vliw->alu1.imm = 0;
                        vliw->alu1.predicate = 0;
                        vliw->alu1.loopStart = -1;
                        vliw->alu1.cycle = 0;
                        vliw->alu1.done = false;
                    } else {
                        // create a new VLIW bundle
                        state->bundles.size += 1;
                        state->bundles.vliw = (VLIW*)realloc(state->bundles.vliw, state->bundles.size * sizeof(VLIW));
                        state->bundles.vliw[state->bundles.size - 1] = {NOPinstr, NOPinstr, NOPinstr, NOPinstr, NOPinstr};

                        VLIW *vliw2 = &state->bundles.vliw[state->bundles.size - 1];
                        vliw2->alu1.type = MOV;
                        vliw2->alu1.src1 = instrs.instructions[entry->ID-65].dest;
                        vliw2->alu1.dest = regDep;
                        // Set the other fields of the MOV instruction
                        vliw2->alu1.imm = -1;
                        vliw2->alu1.predicate = false;
                        vliw2->alu1.loopStart = -1;
                        vliw2->alu1.cycle = 0;

                        // move the loop instruction to the new VLIW bundle
                        vliw2->br = vliw->br;
                        vliw->br = NOPinstr; 
                    }
                }
            }            
        }
    }

    // Step 4. allocate unused register for all source registers that have no RAW dependency
    // for every instruction in DependencyTable, check if it has a dependency on another instruction
    // if not, allocate a register to the source register
    for (int j = 0; j < table->size; j++) {
        DependencyEntry *entry = table->dependencies[j];
        if (entry->local.size == 0 && entry->interloop.size == 0 && entry->invariant.size == 0 && entry->postL.size == 0) {
            // allocate a register to the source register
            entry->src1 = reg;
            reg++;
        }
    }

}

/**
 * @brief Create a new VLIW bundle.
 * @param state The pointer to the processor state.
 * @return VLIW The new VLIW bundle.
 */
VLIW newVLIW(ProcessorState *state) {
    // realloc bundles
    state->bundles.size += 1;
    state->bundles.vliw = (VLIW*)realloc(state->bundles.vliw, state->bundles.size * sizeof(VLIW));
    state->bundles.vliw[state->bundles.size - 1] = {NOPinstr, NOPinstr, NOPinstr, NOPinstr, NOPinstr};

    return state->bundles.vliw[state->bundles.size - 1];
}

typedef struct {
    int latestALU1;
    int latestALU2;
    int latestMult;
    int latestMem;
    int latestBr;
} SchedulerState;

/**
 * @brief Schedule an instruction in the VLIW bundles, update indices.
 * @param state The state of the processor.
 * @param entry The dependency entry to schedule.
 * @param latestALU1 The latest used ALU1 index.
 * @param latestALU2 The latest used ALU2 index.
 * @param latestMult The latest used MULT index.
 * @param latestMem The latest used MEM index.
 * @param latestBr The latest used BR index.
 */
void scheduleInstruction(ProcessorState *state, DependencyEntry *entry, SchedulerState *schedulerState) {
    bool scheduled = false;
    VLIW *vliw = NULL; 

    // Example of why restoring the old latest counters is important 

    // ADDI
    // ST
    // LD
    // ADD   with RAW dependency on LD
    // ADDI
    // SUB
    // ADD
    // ADD
    // ADDI

    //****************************************        
    //     ALU1   ALU2   MULU   MEM   BR      
    //     
    // 0   ADDI   NOP    NOP    ST    NOP     latestALU1 = 1, latestALU2 = 0
    //
    //****************************************

    //****************************************        
    //     ALU1   ALU2   MULU   MEM   BR      
    //     
    // 0   ADDI   NOP    NOP    ST    NOP     latestALU1 = 1, latestALU2 = 0
    // 1   NOP    NOP    NOP    LD    NOP
    //
    //****************************************

    //****************************************        
    //     ALU1   ALU2   MULU   MEM   BR      
    //     
    // 0   ADDI   NOP    NOP    ST    NOP     latestALU1 = 1, latestALU2 = 0
    // 1   NOP    NOP    NOP    LD    NOP
    // 2   ADD    NOP    NOP    NOP   NOP     latestALU1 NOT changed by dependency (!)
    //
    //****************************************

    //****************************************        
    //     ALU1   ALU2   MULU   MEM   BR      
    //     
    // 0   ADDI   ADDI   NOP    ST    NOP     latestALU1 = 1, latestALU2 = 1
    // 1   NOP    NOP    NOP    LD    NOP
    // 2   ADD    NOP    NOP    NOP   NOP     
    //
    //****************************************

    //****************************************        
    //     ALU1   ALU2   MULU   MEM   BR      
    //     
    // 0   ADDI   ADDI   NOP    ST    NOP     latestALU1 = 2, latestALU2 = 1
    // 1   SUB    NOP    NOP    LD    NOP
    // 2   ADD    NOP    NOP    NOP   NOP
    //
    //****************************************

    //****************************************        
    //     ALU1   ALU2   MULU   MEM   BR      
    //     
    // 0   ADDI   ADDI   NOP    ST    NOP     latestALU1 = 2, latestALU2 = 2
    // 1   SUB    ADD    NOP    LD    NOP
    // 2   ADD    NOP    NOP    NOP   NOP
    //
    //****************************************

    //****************************************        
    //     ALU1   ALU2   MULU   MEM   BR      
    //     
    // 0   ADDI   ADDI   NOP    ST    NOP     latestALU1 = 3, latestALU2 = 2
    // 1   SUB    ADD    NOP    LD    NOP
    // 2   ADD    NOP    NOP    NOP   NOP     latestALU1 incremented
    //
    //****************************************

    //****************************************        
    //     ALU1   ALU2   MULU   MEM   BR      
    //     
    // 0   ADDI   ADDI   NOP    ST    NOP     latestALU1 = 3, latestALU2 = 3
    // 1   SUB    ADD    NOP    LD    NOP
    // 2   ADD    ADD    NOP    NOP   NOP      
    //
    //****************************************

    //****************************************        
    //     ALU1   ALU2   MULU   MEM   BR      
    //     
    // 0   ADDI   ADDI   NOP    ST    NOP     latestALU1 = 4, latestALU2 = 3
    // 1   SUB    ADD    NOP    LD    NOP
    // 2   ADD    ADD    NOP    NOP   NOP      
    // 3   ADDI   NOP    NOP    NOP   NOP
    //
    //****************************************

    if (entry->type == ADD || entry->type == ADDI || entry->type == SUB || entry->type == MOV) {
        while (schedulerState->latestALU1 < state->bundles.size || schedulerState->latestALU2 < state->bundles.size) { // latestALU1 == latestALU2 for dependencies
            if (schedulerState->latestALU1 < state->bundles.size && schedulerState->latestALU1 <= schedulerState->latestALU2) { // ALU1 only chosen if it's not later than a free ALU2
                if (vliw->alu1.type == NOP) {
                    vliw = &state->bundles.vliw[schedulerState->latestALU1]; // take the vliw bundle at the latestALU1 index
                    vliw->alu1 = &instrs.instructions[entry->ID-65];
                    entry->scheduledTime = schedulerState->latestALU1;
                    schedulerState->latestALU1 += 1;

                    scheduled = true;
                    break;
                } else {
                    schedulerState->latestALU1 += 1;
                }
            } else if (schedulerState->latestALU2 < state->bundles.size) {
                if (vliw->alu2.type == NOP) {
                    vliw = &state->bundles.vliw[schedulerState->latestALU2];
                    vliw->alu2 = &instrs.instructions[entry->ID-65];
                    entry->scheduledTime = schedulerState->latestALU2;
                    schedulerState->latestALU2 += 1;

                    scheduled = true;
                    break; 
                } else {
                    schedulerState->latestALU2 += 1;
                }
            }
        } if (!scheduled) {
            vliw = &newVLIW(state);
            vliw->alu1 = &instrs.instructions[entry->ID-65];
            entry->scheduledTime = schedulerState->latestALU1; // latestALU1 will be == the size of the bundles-1
            schedulerState->latestALU1 += 1;
        }
    } else if (entry->type == MULU) {
        while (schedulerState->latestMult < state->bundles.size) {
            if (vliw->mult.type == NOP) {
                vliw = &state->bundles.vliw[schedulerState->latestMult];
                vliw->mult = &instrs.instructions[entry->ID-65];
                entry->scheduledTime = schedulerState->latestMult;
                schedulerState->latestMult += 1;

                scheduled = true;
                break;
            }
            schedulerState->latestMult += 1;
        } if (!scheduled) {
            vliw = &newVLIW(state);
            vliw->mult = &instrs.instructions[entry->ID-65];
            entry->scheduledTime = schedulerState->latestMult;
            schedulerState->latestMult += 1;
        }
    } else if (entry->type == LD || entry->type == ST) {
        while (schedulerState->latestMem < state->bundles.size) {
            if (vliw->mem.type == NOP) {
                vliw = &state->bundles.vliw[schedulerState->latestMem];
                vliw->mem = &instrs.instructions[entry->ID-65];
                entry->scheduledTime = schedulerState->latestMem;
                schedulerState->latestMem += 1;

                scheduled = true;
                break;
            }
            schedulerState->latestMem += 1;
        } if (!scheduled) {
            vliw = &newVLIW(state);
            vliw->mem = &instrs.instructions[entry->ID-65];
            entry->scheduledTime = schedulerState->latestMem;
            schedulerState->latestMem += 1;
        }

    } 

    // update the latest loop instruction to make sure the branch is NOT placed earlier than any other instruction in the loop
    if (instrs.instructions[entry->ID-65].block == 1 && entry->type != LOOP && entry->type != LOOP_PIP) { 
        schedulerState->latestBr = math.max(schedulerState->latestBr, entry->scheduledTime);
    }
    
    // BR instruction must always be scheduled in LAST bundle of BB1 
    else if (entry->type == LOOP || entry->type == LOOP_PIP) {
        while (schedulerState->latestBr < state->bundles.size) {
            if (vliw->br.type == NOP) { // probably not necessary 
                vliw = &state->bundles.vliw[schedulerState->latestBr];
                vliw->br = &instrs.instructions[entry->ID-65];
                entry->scheduledTime = schedulerState->latestBr;
                schedulerState->latestBr += 1;

                scheduled = true;
                break;
            }
            schedulerState->latestBr += 1;
        } if (!scheduled) {
            vliw = &newVLIW(state);
            vliw->br = &instrs.instructions[entry->ID-65];
            entry->scheduledTime = schedulerState->latestBr;
            schedulerState->latestBr += 1;
        }
    }
}

/**
 * @brief Schedule instructions in the VLIW bundles based on the DependencyTable.
 * @param state Pointer to the processor state.
 * @param table Pointer to the DependencyTable.
 */
void scheduleInstructions(ProcessorState *state, DependencyTable *table) {
    // Schedule instructions in the VLIW bundle based on the DependencyTable

    SchedulerState schedulerState;
    schedulerState.latestALU1 = 0;
    schedulerState.latestALU2 = 0;
    schedulerState.latestMult = 0;
    schedulerState.latestMem = 0;
    schedulerState.latestBr = 0;
    schedulerState.BB1 = 0; 
    schedulerState.BB2 = 0;

    // create a new VLIW bundle
    VLIW *vliw = &newVLIW(state);

    for (int i = 0; i < table->size; i++) {
        DependencyEntry *entry = table->dependencies[i];

        // When enter BB1 or BB2, stop using the previous VLIW bundles and start a new one
        // and set the latest counters to the size of the bundles-1
        if ((entry->block == 1 && table->dependencies[i-1]->block == 0) ||    // first instruction in BB1
            (entry->block == 2 && table->dependencies[i-1]->block == 1)) {    // first instruction in BB2
            vliw = &newVLIW(state);
            schedulerState.latestALU1 = state->bundles.size - 1;
            schedulerState.latestALU2 = state->bundles.size - 1;
            schedulerState.latestMult = state->bundles.size - 1;
            schedulerState.latestMem  = state->bundles.size - 1;
            schedulerState.latestBr   = state->bundles.size - 1;
        }

        // If there are no local, invariant or post loop dependencies (CAN have interloop dependencies !)
        if (entry->local.size == 0 && entry->invariant.size == 0 && entry->postL.size == 0) {
            // Schedule the instruction in the VLIW bundle

            // updated list of latest used ALU1, ALU2, MULT, MEM, BR, and loop instruction indices
            scheduleInstruction(state, entry, &schedulerState);
        }

//-->   // Local, invariant or post loop dependencies (at least 1)
        else {
            // check the LATEST schedule time of the instruction(s) it depends on
            LatestDependency latestDep = findLatestDependency(table, entry);

            int latestScheduledTime = latestDep.scheduledTime;
            int IDdependsOn = latestDep.idx;
            
            // scheduling cases for different instruction types

            // schedule the instruction in the first VLIW bundle with the available correct unit and the latest schedule time
            if (table->dependencies[IDdependsOn].type == MULU) {
                int oldLatest = schedulerState.latestMult; // save copy of old latest
                // new entry must be scheduled AFTER its dependency is resolved (latestScheduledTime + latency) or later 
                // only need to do +1 because +2 was already done when computing latestScheduledTime
                schedulerState.latestMult = (schedulerState.latestMult < latestScheduledTime + 1) ? latestScheduledTime + 1 : schedulerState.latestMult; // compute latestMult making sure the new entry is scheduled AT LEAST 3 cycles AFTER its dependency
                // create as many new VLIW bundles as the difference between the old latest and the new latest
                for (int j = oldLatest; j < schedulerState.latestMult; j++) {
                    vliw = &newVLIW(state);
                }
                // schedule the instruction in the VLIW bundle with the latest scheduled time
                vliw->mult = &instrs.instructions[entry->ID-65];
                entry->scheduledTime = schedulerState.latestMult;
                scheduleInstruction(state, entry, &schedulerState);
                schedulerState.latestMult = oldLatest; // restore old latest
            }
            else if (table->dependencies[IDdependsOn].type == LD || table->dependencies[IDdependsOn].type == ST) {
                int oldLatest = schedulerState.latestMem;
                schedulerState.latestMem = math.max(schedulerState.latestMem, latestScheduledTime + 1); 
                scheduleInstruction(state, entry, &schedulerState);
                schedulerState.latestMem = oldLatest;
            }
            else if (table->dependencies[IDdependsOn].type == LOOP || table->dependencies[IDdependsOn].type == LOOP_PIP) {  // TODO does this case exist ?
                int oldLatest = schedulerState.latestBr;
                schedulerState.latestBr = math.max(schedulerState.latestBr, latestScheduledTime + 1);
                scheduleInstruction(state, entry, &schedulerState);
                schedulerState.latestBr = oldLatest;
            }
            else {
                int oldLatest1 = schedulerState.latestALU1;
                int oldLatest2 = schedulerState.latestALU2;
                schedulerState.latestALU1 = math.max(schedulerState.latestALU1, latestScheduledTime + 1);
                schedulerState.latestALU2 = math.max(schedulerState.latestALU2, latestScheduledTime + 1);
                scheduleInstruction(state, entry, &schedulerState);
                schedulerState.latestALU1 = oldLatest1;
                schedulerState.latestALU2 = oldLatest2;
            }
        }

//-->   // Interloop dependencies
        // handled right after all of BB1 is scheduled
        if (table->dependencies[instrs.loopEnd].scheduledTime != -1 && table->dependencies[instrs.loopEnd+1].scheduledTime == -1) {
            for (int i = instrs.loopStart; i < instrs.loopEnd; i++) { // TODO Marin loopEnd or loopEnd+1 ?
                DependencyEntry *entry = table->dependencies[i];

                if (entry->loop.size != 0) {
                    int latestScheduledTime = -1;
                    int IDdependsOn = -1;
                    // look for the latest scheduled time of the instruction(s) it depends on ONLY in the loop (i.e. block == 1)
                    for (int j = 0; j < entry->loop.size; j++) {
                        if (table->dependencies[entry->loop.list[j].ID-65].type == MULU) {
                            if ((table->dependencies[entry->loop.list[j].ID-65].scheduledTime + 2) > latestScheduledTime) {
                                latestScheduledTime = table->dependencies[entry->loop.list[j].ID-65].scheduledTime + 2;
                                IDdependsOn = entry->loop.list[j].ID-65;
                            }
                        } else if (table->dependencies[entry->loop.list[j].ID-65].scheduledTime > latestScheduledTime) {
                            latestScheduledTime = table->dependencies[entry->loop.list[j].ID-65].scheduledTime;
                            IDdependsOn = entry->loop.list[j].ID-65;
                        }
                    }

                    // only case that needs to be handled: depends on a MULU
                    if (table->dependencies[IDdependsOn].type == MULU) {
                        // check that the distance between this instruction and the entry is at least 3 cycles
                        
                        // distance between the beginning of the loop and the instruction scheduled time
                        int x = entry->scheduledTime - instrs.loopStart;
                        // distance between the end of the loop scheduled time and the instruction it depends on
                        int y = table->dependencies[instrs.loopEnd].scheduledTime - table->dependencies[IDdependsOn].scheduledTime;

                        // if the distance is less than 3, need to add NOP cycles
                        for (int j = 0; j < (3 - (x + y + 1)); j++) {
                            vliw = &newVLIW(state);
                        }
                        // postpone the LOOP or LOOP_PIP instruction to the end of the loop
                        vliw     = &state->bundles.vliw[table->dependencies[instrs.loopEnd].scheduledTime];
                        vliw->br = &NOPinstr; 

                        vliw     = &state->bundles.vliw[state->bundles.size-1];
                        vliw->br = &instrs.instructions[instrs.loopEnd];

                        schedulerState.latestBr = state->bundles.size-1;
                        table->dependencies[instrs.loopEnd].scheduledTime = schedulerState.latestBr;
                    }
                }

            }
        }
    }
}

            // // only case that needs to be handled: depends on a MULU
            // if (table->dependencies[IDdependsOn].type == MULU) {
            //     // check that the distance between this instruction and the entry is at least 3 cycles
                
            //     // distance between the beginning of the loop and the instruction scheduled time
            //     int x = entry->scheduledTime - instrs.loopStart;
            //     // distance between the end of the loop scheduled time and the instruction it depends on
            //     int y = table->dependencies[instrs.loopEnd].scheduledTime - table->dependencies[IDdependsOn].scheduledTime;

            //     // if the distance is less than 3, need to add NOP cycles
            //     if ((x + y + 1) < 3) {  // TODO +1 ? 
            //         // push down all VLIW bundles of the BB2 and insert NOPs at the end of BB1
            //         int pushDown = 3 - (x + y + 1);
            //         int j = table->dependencies[instrs.loopEnd].scheduledTime + 1;
                    
            //         // save the j+pushdown-1 VLIW bundles of BB2
            //         VLIW *temp = (VLIW*)malloc(pushDown * sizeof(VLIW));
            //         for (int k = 0; k < pushDown; k++) {
            //             temp[k] = state->bundles.vliw[j+k]; 
            //         }
            //         // insert NOPs at the end of BB1
            //         for (int k = 0; k < pushDown; k++) {
            //             state->bundles.vliw[j+k] = {NOP, NOP, NOP, NOP, NOP};
            //         }
            //         // move the VLIW bundles of BB2 down
            //         // save the last three VLIW bundles
            //          VLIW *last = (VLIW*)malloc(pushDown * sizeof(VLIW));
            //         for (int k = 0; k < pushDown; k++) {
            //             last[k] = state->bundles.vliw[state->bundles.size-1-k];
            //         }

            //         for (int k = state->bundles.size-1-pushDown; k >= j+pushDown; k--) {
            //             state->bundles.vliw[k] = state->bundles.vliw[k-pushDown];
            //         }
            //         // restore the last three VLIW bundles
            //         for (int k = 0; k < pushDown; k++) {








//*************************************************************
//
//                     Pipeline Stages
//
//*************************************************************


//     // Check and adjust the Initiation Interval (II) for the instruction
//     // based on interloop dependencies
//     // checkInterloopDependencies(table, state);  // TODO no loops yet

//     // Perform register allocation
//     ...
// }

// /**
//  * Execute stage of the pipeline.
//  * @param state Pointer to the processor state.
//  */
// void Execute(ProcessorState *state) {
//     // for the instruction in the VLIW bundle with scheduled time = PC, execute it
//     // if done update the register file

//     // ALU1
//     state->bundles.vliw[state->PC].alu1.cycle = 1;
//     state->bundles.vliw[state->PC].alu1.done  = true;  // even if NOP 
//     switch (state->bundles.vliw[state->PC].alu1.type) {
//         case ADD:
//             state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src1] + state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src2];
//             break;
//         case ADDI:
//             state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src1] + state->bundles.vliw[state->PC].alu1.imm;
//             break;
//         case SUB:
//             state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src1] - state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src2];
//             break;
//         case MOV:
//             if (strcmp(state->bundles.vliw[state->PC].alu1.dest, "LC") == 0) {  // case of mov LC
//                 state->LC = state->bundles.vliw[state->PC].alu1.src1;
//                 // state->EC = #loop stages i.e. #loop iterations
//                 state->Ec = state->bundles.vliw[state->PC].alu2.src1; // TODO check if it's the right way to do it
//             } else if (state->bundles.vliw[state->PC].alu1.imm != -1) {  // case of mov with immediate
//                 state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->bundles.vliw[state->PC].alu1.imm;
//             } else {  // case of mov with register
//                 state->PhysRegFile[state->bundles.vliw[state->PC].alu1.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu1.src1];
//             }
//             break;
//         default:
//             break;
//     }


//     // ALU2
//     state->bundles.vliw[state->PC].alu2.cycle = 1;
//     state->bundles.vliw[state->PC].alu2.done  = true; 
//     switch (state->bundles.vliw[state->PC].alu2.type) {
//         case ADD:
//             state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src1] + state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src2];
//             break;
//         case ADDI:
//             state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src1] + state->bundles.vliw[state->PC].alu2.imm;
//             break;
//         case SUB:
//             state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src1] - state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src2];
//             break;
//         case MOV:
//             if (strcmp(state->bundles.vliw[state->PC].alu2.dest, "LC") == 0) {  // case of mov LC
//                 state->LC = state->bundles.vliw[state->PC].alu2.src1;
//                 // state->EC = #loop stages i.e. #loop iterations
//                 state->Ec = state->bundles.vliw[state->PC].alu2.src1; // TODO check if it's the right way to do it 
//             } else if (state->bundles.vliw[state->PC].alu2.imm != -1) {  // case of mov with immediate
//                 state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->bundles.vliw[state->PC].alu2.imm;
//             } else {  // case of mov with register
//                 state->PhysRegFile[state->bundles.vliw[state->PC].alu2.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].alu2.src1];
//             }
//             break;
//         default:
//             break;
//     }

//     // MULT
//     if (state->bundles.vliw[state->PC - 1].mult.done == false && state->bundles.vliw[state->PC - 1].mult.type == MULU) {
//         // if NOP and previous instruction was an UNFINISHED MULU, copy the mult instruction from the previous cycle
//         state->bundles.vliw[state->PC].mult = state->bundles.vliw[state->PC - 1].mult;   // TODO by refernece .... but it's what we want 
//     }
//     if (state->bundles.vliw[state->PC].mult.type == MULU) {  // if MULU, start/keep executing it 
//         state->bundles.vliw[state->PC].mult.cycle += 1;
//         if (state->bundles.vliw[state->PC].mult.cycle == 3) {
//             state->bundles.vliw[state->PC].mult.done = true;
//             state->PhysRegFile[state->bundles.vliw[state->PC].mult.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].mult.src1] * state->PhysRegFile[state->bundles.vliw[state->PC].mult.src2];
//         }
//     } else { // if NOP, set done to true 
//         // (this is the case where NO MULU instructions are present in the VLIW bundle
//         // or the previous MULU instruction is done)
//         state->bundles.vliw[state->PC].mult.done = true;
//     }

//     //********************        //********************
//     //       MULU                 //    MULU cycle 1
//     //       NOP             =>   //    MULU cycle 2
//     //       NOP                  //    MULU cycle 3, done
//     //       NOP                  //    NOP  done 
//     //********************        //********************

//     //********************        //********************
//     //       NOP                  //    NOP  done
//     //       NOP                  //    NOP  done
//     //       MULU                 //    MULU cycle 1
//     //       NOP             =>   //    MULU cycle 2
//     //       NOP                  //    MULU cycle 3, done
//     //       NOP                  //    NOP  done 
//     //********************        //********************

//     // LD 
//     state->bundles.vliw[state->PC].mem.cycle = 1;
//     state->bundles.vliw[state->PC].mem.done  = true;
//     if (state->bundles.vliw[state->PC].mem.type == LD) {
//         state->PhysRegFile[state->bundles.vliw[state->PC].mem.dest] = state->PhysRegFile[state->bundles.vliw[state->PC].mem.src1] + state->bundles.vliw[state->PC].mem.imm;
//     }  // TODO store operations are not considered ?? 

//     // BR (LOOP, LOOP_PIP)
//     state->bundles.vliw[state->PC].br.cycle = 1;
//     state->bundles.vliw[state->PC].br.done  = true;
//     if (state->bundles.vliw[state->PC].br.type == LOOP) {  // unconditional branch
//         state->PC = state->bundles.vliw[state->PC].br.loopStart;   
//     } else if (state->bundles.vliw[state->PC].br.type == LOOP_PIP) {  // conditional branch
//         if (state->LC > 0) {
//             state->LC -= 1;
//             // state->EC unchanged
//             state->RRB += 1;
//             // enable stage predicate register  TODO ????? always p32 ? 
//             state->PredRegFile[31] = true;
//             state->PC = state->bundles.vliw[state->PC].br.loopStart;
//         } else {
//             if (state->EC > 0) {
//                 // state->LC unchanged
//                 state->EC -= 1;
//                 state->RRB += 1;
//                 // disable stage predicate register  TODO ????? 
//                 state->PredRegFile[31] = false;
//                 state->PC = state->bundles.vliw[state->PC].br.loopStart;
//             } else {
//                 //state->PC += 1;  // TODO nothing ? bc was already incremented by FetchAndDecode ? 
//             }
//         }
//     }
// }

// /**
//  * Commit stage of the pipeline.
//  * @param state Pointer to the processor state.
//  */
// void Commit(ProcessorState *state, DependencyTable *table) {
//     // Commit the instructions in the VLIW bundle
//     // if done update the register file
//     // TODO how to update DepTable? 
//     // TODO
//     if (state->bundles.vliw[state->PC].alu1.done) {
//         state->bundles.vliw[state->PC].alu1.done = false;
//     }
//     if (state->bundles.vliw[state->PC].alu2.done) {
//         state->bundles.vliw[state->PC].alu2.done = false;
//     }
//     if (state->bundles.vliw[state->PC].mult.done) {
//         state->bundles.vliw[state->PC].mult.done = false;
//     }
//     if (state->bundles.vliw[state->PC].mem.done) {
//         state->bundles.vliw[state->PC].mem.done = false;
//     }
//     if (state->bundles.vliw[state->PC].br.done) {
//         state->bundles.vliw[state->PC].br.done = false;
//     }
// }



/*************************************************************
 * 
 *                    Parsing Methods
 * 
 * ***********************************************************/

/**
 * @brief Add an instruction to the instruction set.
 */
void pushInstruction(InstructionEntry entry) {
    instrs.instructions = realloc(instrs.instructions, (instrs.size + 1) * sizeof(InstructionEntry));

    instrs.instructions[instrs.size] = entry;
    instrs.size++;
}

/**
 * @brief Parse the input file and store the instructions in the instruction set.
 */
void parseInstrunctions(char *progFile, char *inputFile) {

    FILE *file = fopen(progFile, "r");
    FILE *file2 = fopen(inputFile, "r");
    printf("%p\n", file);
    if (file == NULL)
    {
        printf("Error opening file\n");
        exit(1);
    }
    if (file2 == NULL)
    {
        printf("Error opening file\n");
        exit(1);
    }

    // size cannot be find like this as inputs are not uniform, can only be found dynamically
    // or if we count the number of strings in the jsons as both files are jsons

    // progFile is an array of 2 arrays different sizes of strings and input file is an array of strings
    //  there is no name prog file looks like this -> [[],[]]

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file); // Go back to the beginning of the file

    fseek(file2, 0, SEEK_END);
    long file_size2 = ftell(file2);
    rewind(file2); // Go back to the beginning of the file

    // Allocate memory to store the file contents
    char *json_data = (char *)malloc(file_size + 1);
    if (json_data == NULL)
    {
        printf("Memory allocation failed.\n");
        fclose(file);
        return;
    }

    char *json_data2 = (char *)malloc(file_size2 + 1);
    if (json_data2 == NULL)
    {
        printf("Memory allocation failed.\n");
        fclose(file2);
        return;
    }
    // Read the file contents into the allocated memory
    fread(json_data, 1, file_size, file);
    json_data[file_size] = '\0'; 

    fread(json_data2, 1, file_size2, file2);
    json_data2[file_size2] = '\0'; 

    // Close the file

    cJSON *root = cJSON_Parse(json_data);
    printf("%s", *root);
    if (root == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            printf("Error before: %s\n", error_ptr);
        }
        cJSON_free(json_data); // Free memory
        return;
    }
    cJSON *root2 = cJSON_Parse(json_data2);

    // root is an array of 2 arrays of strings
    cJSON *prog1 = cJSON_GetArrayItem(root, 0);
    cJSON *prog2 = cJSON_GetArrayItem(root, 1);

    // prog1 is an array of strings
    for (int i = 0; i < cJSON_GetArraySize(prog1)-1; i++)
    {
        cJSON *instr = cJSON_GetArrayItem(prog1, i);
        // convert instr to string
        char *instr_str = cJSON_Print(instr);
        // parse instr_str to InstructionEntry eg: "addi x1, x1, 1"
        InstructionEntry entry;
     
        parseString(instr_str, &entry);

        pushInstruction(entry);
 
        // parse using tokens
    }
    printf("root2\n");
    for (int i = 0; i < cJSON_GetArraySize(root2); i++)
    {
        cJSON *instr = cJSON_GetArrayItem(root2, i);
        // convert instr to string
        char *instr_str = cJSON_Print(instr);
        // parse instr_str to InstructionEntry eg: "addi x1, x1, 1"
        InstructionEntry entry;
      
        parseString(instr_str, &entry);
        pushInstruction(entry);
        if (entry.opcode[0] == 'l')
        {
            instrs.loopStart = entry.imm;
            instrs.loopEnd = i;
        }
        // parse using tokens
    }

    for (int i = 0; i < cJSON_GetArraySize(prog2); i++)
    {
        cJSON *instr = cJSON_GetArrayItem(prog2, i);
        // convert instr to string
        char *instr_str = cJSON_Print(instr);
        // parse instr_str to InstructionEntry eg: "addi x1, x1, 1"
        InstructionEntry entry;
        
        parseString(instr_str, &entry);
  
        pushInstruction(entry);
      
        // parse using tokens
    }

    // reread the entries if i< loopStart then block =0, if i>= loopStart and i<= loopEnd then block =1, if i> loopEnd then block =2
    for (int i = 0; i < instrs.size; i++)
    {
        if (i < instrs.loopStart)
        {
            instrs.instructions[i].block = 0;
        }
        else if (i >= instrs.loopStart && i <= instrs.loopEnd)
        {
            instrs.instructions[i].block = 1;
        }
        else
        {
            instrs.instructions[i].block = 2;
        }
    } 

    fclose(file);
    fclose(file2);
}

/*************************************************************
 * 
 *                    Display Methods
 * 
 * ***********************************************************/

/**
 * @brief Function to display one instruction.
 */
void showInstruction(InstructionEntry instr) {
    printf("Instruction\n");
    printf("Opcode: %s\n", instr.opcode);
    printf("Block: %d\n", instr.block);
    printf("Dest: %d\n", instr.dest);
    printf("Src1: %d\n", instr.src1);
    printf("Src2: %d\n", instr.src2);
    printf("Imm: %d\n", instr.imm);
    printf("Pred: %d\n", instr.predicate);
    printf("Type: %d\n", instr.type);
    printf("Cycle: %d\n", instr.cycle);
    printf("Done: %d\n", instr.done);
}

/**
 * @brief Function to display the instruction set.
 */
void showInstructions() {
    printf("Instructions\n");
    printf("Size: %d\n", instrs.size);
    printf("Loop Start: %d\n", instrs.loopStart);
    printf("Loop End: %d\n", instrs.loopEnd);
    
    for (int i = 0; i < instrs.size; i++) {
        printf("Instruction %d\n", i);
        showInstruction(instrs.instructions[i]);
    }
}

/**
 * @brief Function to display the dependency table.
 */
void showDepTable(DependencyTable table)
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

/**
 * @brief Function to display one VLIW bundle.
 */
void showVLIW(VLIW vliw) {
    printf("ALU1\n");
    showInstruction(vliw.alu1);
    printf("ALU2\n");
    showInstruction(vliw.alu2);
    printf("MULT\n");
    showInstruction(vliw.mult);
    printf("MEM\n");
    showInstruction(vliw.mem);
    printf("BR\n");
    showInstruction(vliw.br);
}

/**
 * @brief Function to display the processor state.
 */
void showProcessorState(ProcessorState state) {
    printf("Processor State\n");
    printf("PC: %d\n", state.PC);
    printf("LC: %d\n", state.LC);
    printf("EC: %d\n", state.EC);
    printf("RRB: %d\n", state.RRB);
    printf("II: %d\n", state.II);
    printf("PredRegFile\n");
    for (int i = 0; i < REGS; i++) {
        printf("%d: %d\n", i, state.PredRegFile[i]);
    }
    printf("PhysRegFile\n");
    for (int i = 0; i < REGS; i++) {
        printf("%d: %d\n", i, state.PhysRegFile[i]);
    }
    printf("Bundles\n");
    for (int i = 0; i < state.bundles.size; i++) {
        printf("VLIW %d\n", i);
        showVLIW(state.bundles.vliw[i]);
    }
}