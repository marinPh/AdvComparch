#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "../lib/cJSON.h"
#include "../lib/cJSON_Utils.h"
#include "../lib/VLIW470.h"
#include "../lib/utils.h"

#define ROTATION_START_INDEX 32 // Start index of rotating registers
#define LOOP_AROUND 64          // Number of registers in the rotating register file
#define max(a, b) ((a) > (b) ? (a) : (b))

InstructionsSet instrs = {NULL, 0};
DependencyTable depTable = {NULL, 0};
ProcessorState state;

InstructionEntry NOPinstr = {"nop", 0, -1, -1, -1, 0, -1, NOP, 0, false};

/*
 * Initialize the processor state.
 */
void initProcessorState(ProcessorState *state)
{
    state->PC = 0;
    state->LC = 0;
    state->EC = 0;
    state->RRB = 0;
    memset(state->PhysRegFile, 0, sizeof(state->PhysRegFile));
    memset(state->PredRegFile, 0, sizeof(state->PredRegFile));
    state->FUCount[0] = 2;
    state->FUCount[1] = 1;
    state->FUCount[2] = 1;
    state->FUCount[3] = 1;
    state->bundles.vliw = NULL;
    state->bundles.size = 0;
    state->II = 1;
    state->stage = 0;
}

/**
 * @brief Add a dependency entry to the dependency table.
 * @param table The pointer to the dependency table.
 * @param entry The dependency entry to add.
 */
void pushDependency(DependencyTable *table, DependencyEntry entry)
{
    table->dependencies = realloc(table->dependencies, (table->size + 1) * sizeof(DependencyEntry));
    table->dependencies[table->size] = entry;
    table->size++;
}

/**
 * @brief Initialize the dependency table with the instructions in the instruction set.
 * @return DependencyTable
 */
DependencyTable dependencyTableInit()
{
    DependencyTable table = {NULL, 0};
    for (int i = 0; i < instrs.size; i++)
    {
        DependencyEntry entry = {i, 65 + i, instrs.instructions[i].type, instrs.instructions[i].dest, {0, {NULL, 0}}, {0, {NULL, 0}}, {0, {NULL, 0}}, {0, {NULL, 0}}, -1};
        pushDependency(&table, entry);
    }
    return table;
}
/**
 * @brief Add a dependency to the list of dependencies.
 * @param list The pointer to the list of dependencies.
 * @param id The ID of the instruction to add as a dependency.
 * @param reg The register that the instruction writes to.
 */
void pushId(idList *list, char id, unsigned int reg,int src)
{
    list->list = realloc(list->list, (list->size + 1) * sizeof(dependency));
    list->list[list->size].ID = id;
    list->list[list->size].reg = reg;
    list->list[list->size].src =src;
    list->size++;
}

/**
 * @brief Check the dependencies between two instructions.
 * @param instr1 The index of the first instruction.
 * @param instr2 The index of the second instruction.
 * @param table The dependency table pointer.
 */
void whatType(int instr1, int instr2, DependencyTable *table)
{
    DependencyEntry *entry = &(table->dependencies[instr2]);
    // printf("instr1: %d, instr2: %d\n", instr1, instr2);
    //  we check if the dest of instr1 is equal to src1 or src2 of instr2
    

  

    if (instrs.instructions[instr1].dest == instrs.instructions[instr2].src1 || instrs.instructions[instr2].src2 == instrs.instructions[instr1].dest)
    {
        //src is the name of the src that has the dependency
        int src = instrs.instructions[instr1].dest == instrs.instructions[instr2].src1 ? 1 : 2;
       
        if (instrs.loopStart != -1)
        { // check if in same block
        

            if (instrs.instructions[instr1].block == instrs.instructions[instr2].block)
            {
                // check if instr1 > instr2

                if (instr1 < instr2)
                {
                    pushId(&entry->local, table->dependencies[instr1].ID, instrs.instructions[instr1].dest,src);
                }
                else
                {
                    pushId(&entry->loop, table->dependencies[instr1].ID, instrs.instructions[instr1].dest,src);
                }
            }
            else
            {
                // check if in post loop means instr1 is in block 1 and instr2 is in block 2
                if (instrs.instructions[instr1].block == 1 && instrs.instructions[instr2].block == 2)
                {
                    pushId(&entry->postL, table->dependencies[instr1].ID, instrs.instructions[instr1].dest,src);
                }
                else
                {
                    // check if in invariant
                    if (instrs.instructions[instr1].block == 0 && (instrs.instructions[instr2].block == 1 || instrs.instructions[instr2].block == 2))
                    {
                        pushId(&entry->invariant, table->dependencies[instr1].ID, instrs.instructions[instr1].dest,src);
                    }
                }
            }
        }
        else
        {
            // if there is no loop all dependencies are local
            pushId(&entry->local, table->dependencies[instr1].ID, instrs.instructions[instr1].dest,src);
        }
    }
    // printf("returning\n");
    // printf("instr1: %d, instr2: %d\n", instr1, instr2);
    return;
}

/**
 * @brief Fill the dependency table with dependencies between instructions.
 *
 * @return DependencyTable
 */
DependencyTable createFillDepencies()
{
    DependencyTable table = dependencyTableInit();
    for (int i = 0; i < table.size; i++)
    {
        // printf("i: %d\n", i);

        
        // printf("i: %d\n", i);

        int pot = instrs.instructions[i].dest;
        // printf("pot: %d\n", pot);
        
        if (pot == -1)
            continue;
          

       
        for (int j = i + 1; j < instrs.size; j++)
        {
            // printf("i: %d, j: %d\n", i, j);
            //  check for each instruction if pot is a dest or src
           
            
            whatType(i, j, &table);
            if (instrs.instructions[j].dest == pot)
                break;
        }

        // if instruction is in block 1 we check for the instructions before in block 1
        // from start of loop to i
        // printf("instr: %s\n", instrs.instructions[i].opcode);

        if (instrs.instructions[i].block == 1 && instrs.loopStart != -1)
        {
            for (int j = instrs.loopStart; j <= i; j++)
            {
                whatType(i, j, &table);
            }
        }
        // printf("instr: %s\n", instrs.instructions[i].opcode);
    }
    // printf("returning\n");
    // showDepTable(table);
    return table;
}

/**
 * Calculate the rotated register index for general-purpose or predicate registers.
 * @param baseIndex The original index of the register.
 * @param rrb The register rotation base.
 * @return The rotated register index.
 */
int getRotatedRegisterIndex(int baseIndex, int rrb)
{
    if (baseIndex >= ROTATION_START_INDEX && baseIndex < REGS)
    {
        int rotatedIndex = baseIndex - rrb;
        // Wrap around if the calculated index goes past the range of rotating registers
        if (rotatedIndex < ROTATION_START_INDEX)
        {
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
int readGeneralRegister(ProcessorState *state, int index)
{
    int rotatedIndex = getRotatedRegisterIndex(index, state->RRB);
    return state->PhysRegFile[rotatedIndex];
}

/**
 * Access a predicate register value with rotation applied.
 * @param state Pointer to the processor state.
 * @param index Index of the predicate register to access.
 * @return The status of the rotated predicate register.
 */
bool readPredicateRegister(ProcessorState *state, int index)
{
    int rotatedIndex = getRotatedRegisterIndex(index, state->RRB);
    return state->PredRegFile[rotatedIndex];
}

/**
 * Lower bound the Initiation Interval (II), which is the number of issue cycles between the start of successive loop iterations.
 * @param set Set of instructions to calculate the II for.
 * @param state Pointer to the processor state containing the FUs available in the processor.
 * @return The lower bound of the II.
 */
int calculateIIRes(ProcessorState *state)
{
    int counts[10] = {0}; // Array to count instruction types

    // Counting instructions by type
    for (int i = instrs.loopStart; i < instrs.loopEnd + 1; i++)
    {
        if (instrs.instructions[i].type != NOP)
        { // Ignore NOPs
            counts[instrs.instructions[i].type]++;
        }
    }

    // Calculation of II based on FU resources
    int II_res = 0;
    II_res = (counts[ADD] + counts[ADDI] + counts[SUB] + counts[MOV] + state->FUCount[0] - 1) / state->FUCount[0];
    II_res = fmax(II_res, (counts[MULU] + state->FUCount[1] - 1) / state->FUCount[1]);
    II_res = fmax(II_res, (counts[LD] + state->FUCount[2] - 1) / state->FUCount[2]);
    II_res = fmax(II_res, (counts[LOOP] + counts[LOOP_PIP] + state->FUCount[3] - 1) / state->FUCount[3]);

    return II_res; // Want to minimize it to have more pipeline parallelism
}

/**
 * @brief Check and adjust the Initiation Interval (II) to satisfy interloop dependencies.
 * @param table Dependency table containing the instructions.
 * @param state Processor state containing the current II.
 * @return int 1 if the II was changed, 0 otherwise.
 */
int checkInterloopDependencies(DependencyTable *table, ProcessorState *state)
{
    int changed = 0;

    // checkAndAdjustIIForInstruction for every instruction in BB1
    for (int i = instrs.loopStart; i < instrs.loopEnd + 1; i++)
    {
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
int checkAndAdjustIIForInstruction(DependencyTable *table, int i, ProcessorState *state)
{
    int changed = 0;

    // Find the instruction in the dependency table
    DependencyEntry *current = &table->dependencies[i];

    // If the instruction has an interloop dependency
    int latencies[10] = {1, 1, 1, 3, 1, 1, 1, 1, 1, 1};

    int latency = 0;
    for (int i = 0; i < current->loop.size; i++)
    {
        DependencyEntry *dependency = &current->loop.list[i];
        // Check the interloop dependency condition
        latency = latencies[dependency->type];
        if (dependency->scheduledTime + latency > current->scheduledTime + state->II)
        {
            // Adjust the II to satisfy the interloop dependency
            state->II = dependency->scheduledTime + latency - current->scheduledTime;
            changed = 1;
        }
    }
    return changed; // Return 1 if the II was changed, 0 otherwise
}

// enum of dependency types
typedef enum
{
    LOCAL,
    INTERLOOP,
    INVARIANT,
    POSTLOOP
} DepType;

typedef struct
{
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
LatestDependency findLatestDependency(DependencyTable *table, DependencyEntry *entry)
{
    LatestDependency latestDep = {-1, -1, -1, -1, -1, -1, LOCAL, LOCAL, -1, -1};

    int latestScheduledTime = -1; // latest scheduled time of the instruction it has a dependency on
    int IDdependsOn = -1;

    int latestScheduledTimeOtherSrc = -1; // latest scheduled time of the instruction it has a dependency on
    int IDdependsOnOtherSrc = -1;

    int reg1 = instrs.instructions[entry->ID - 65].src1; // source register 1
    int reg2 = instrs.instructions[entry->ID - 65].src2; // source register 2

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

    for (int j = 0; j < entry->local.size; j++)
    {
        if (entry->local.list[j].reg == reg1)
        {
            // printf("1---->entry->local.list[j].ID-65: %d\n", entry->local.list[j].ID-65);
            if (table->dependencies[entry->local.list[j].ID - 65].type == MULU)
            {
                if ((table->dependencies[entry->local.list[j].ID - 65].scheduledTime + 2) > latestScheduledTime)
                {
                    latestScheduledTime = table->dependencies[entry->local.list[j].ID - 65].scheduledTime + 2;
                    IDdependsOn = entry->local.list[j].ID - 65;
                }
            }
            else if (table->dependencies[entry->local.list[j].ID - 65].scheduledTime > latestScheduledTime)
            {
                latestScheduledTime = table->dependencies[entry->local.list[j].ID - 65].scheduledTime;
                IDdependsOn = entry->local.list[j].ID - 65;
            }
            latestDep.depType = LOCAL;
        }

        if (entry->local.list[j].reg == reg2)
        {
            // printf("2---->entry->local.list[j].ID-65: %d\n", entry->local.list[j].ID-65);
            if (table->dependencies[entry->local.list[j].ID - 65].type == MULU)
            {
                if ((table->dependencies[entry->local.list[j].ID - 65].scheduledTime + 2) > latestScheduledTimeOtherSrc)
                {
                    latestScheduledTimeOtherSrc = table->dependencies[entry->local.list[j].ID - 65].scheduledTime + 2;
                    IDdependsOnOtherSrc = entry->local.list[j].ID - 65;
                }
            }
            else if (table->dependencies[entry->local.list[j].ID - 65].scheduledTime > latestScheduledTimeOtherSrc)
            {
                latestScheduledTimeOtherSrc = table->dependencies[entry->local.list[j].ID - 65].scheduledTime;
                IDdependsOnOtherSrc = entry->local.list[j].ID - 65;
            }
            latestDep.depTypeOtherSrc = LOCAL;
        }
    }

    //printf("index other depends on: %d\n", IDdependsOnOtherSrc);

    // invariantDeps
    for (int j = 0; j < entry->invariant.size; j++)
    {
        printf("---->entry->invariant.list[j].ID-65: %d\n", entry->ID-65);
        if (entry->invariant.list[j].reg == reg1)
        {
            if (table->dependencies[entry->invariant.list[j].ID - 65].type == MULU)
            {
                if ((table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime + 2) > latestScheduledTime)
                {
                    latestScheduledTime = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime + 2;
                    IDdependsOn = entry->invariant.list[j].ID - 65;
                }
            }
            else if (table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime > latestScheduledTime)
            {
                latestScheduledTime = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime;
                IDdependsOn = entry->invariant.list[j].ID - 65;
            }
            latestDep.depType = INVARIANT;
            // latestDep.block = instrs.instructions[IDdependsOn].block;
        }
        if (entry->invariant.list[j].reg == reg2)
        {
            if (table->dependencies[entry->invariant.list[j].ID - 65].type == MULU)
            {
                if ((table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime + 2) > latestScheduledTimeOtherSrc)
                {
                    latestScheduledTimeOtherSrc = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime + 2;
                    IDdependsOnOtherSrc = entry->invariant.list[j].ID - 65;
                }
            }
            else if (table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime > latestScheduledTimeOtherSrc)
            {
                latestScheduledTimeOtherSrc = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime;
                IDdependsOnOtherSrc = entry->invariant.list[j].ID - 65;
            }
            latestDep.depTypeOtherSrc = INVARIANT;
            // latestDep.blockOtherSrc = instrs.instructions[IDdependsOnOtherSrc].block;
        }
    }

    // postloopDeps
    for (int j = 0; j < entry->postL.size; j++)
    { // only if in BB2
        // // case where entry depends on both BB0 and BB1 => set BB1 dependency       // TODO
        // if (latestDep.depType == INVARIANT && latestDep.block == 0) {   // not finished but I don't think it's necessary
        //     continue;
        // }
        if (entry->postL.list[j].reg == reg1)
        {
            if (table->dependencies[entry->postL.list[j].ID - 65].type == MULU)
            {
                if ((table->dependencies[entry->postL.list[j].ID - 65].scheduledTime + 2) > latestScheduledTime)
                {
                    latestScheduledTime = table->dependencies[entry->postL.list[j].ID - 65].scheduledTime + 2;
                    IDdependsOn = entry->postL.list[j].ID - 65;
                }
            }
            else if (table->dependencies[entry->postL.list[j].ID - 65].scheduledTime > latestScheduledTime)
            {
                latestScheduledTime = table->dependencies[entry->postL.list[j].ID - 65].scheduledTime;
                IDdependsOn = entry->postL.list[j].ID - 65;
            }
            latestDep.depType = POSTLOOP;
        }
        if (entry->postL.list[j].reg == reg2)
        {
            if (table->dependencies[entry->postL.list[j].ID - 65].type == MULU)
            {
                if ((table->dependencies[entry->postL.list[j].ID - 65].scheduledTime + 2) > latestScheduledTimeOtherSrc)
                {
                    latestScheduledTimeOtherSrc = table->dependencies[entry->postL.list[j].ID - 65].scheduledTime + 2;
                    IDdependsOnOtherSrc = entry->postL.list[j].ID - 65;
                }
            }
            else if (table->dependencies[entry->postL.list[j].ID - 65].scheduledTime > latestScheduledTimeOtherSrc)
            {
                latestScheduledTimeOtherSrc = table->dependencies[entry->postL.list[j].ID - 65].scheduledTime;
                IDdependsOnOtherSrc = entry->postL.list[j].ID - 65;
            }
            latestDep.depTypeOtherSrc = POSTLOOP;
        }
    }

    // switch the dependencies found if latestScheduledTimeOtherSrc > latestScheduledTime
    // latestScheduledTime must always be the GREATEST scheduled time
    if (latestScheduledTimeOtherSrc > latestScheduledTime)
    {
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

    latestDep.idx = IDdependsOn;
    latestDep.dest = table->dependencies[IDdependsOn].dest;
    latestDep.scheduledTime = latestScheduledTime;
    latestDep.block = instrs.instructions[IDdependsOn].block;

    //printf("lastestDep other src: %d\n", latestDep.idxOtherSrc);
    latestDep.idxOtherSrc = IDdependsOnOtherSrc;
    latestDep.destOtherSrc = table->dependencies[IDdependsOnOtherSrc].dest;
    latestDep.scheduledTimeOtherSrc = latestScheduledTimeOtherSrc;
    latestDep.blockOtherSrc = instrs.instructions[IDdependsOnOtherSrc].block;

    return latestDep;
}

InstructionEntry *createNewInstruction(
    char *opcode,
    int dest,
    int src1,
    int src2,
    int imm,
    int predicate,
    InstructionType type,
    int cycle,
    bool done)
{
InstructionEntry *instr = malloc(sizeof(InstructionEntry));
    strcpy(instr->opcode, opcode);
    instr->dest = dest;
    instr->src1 = src1;
    instr->src2 = src2;
    instr->imm = imm;
    instr->predicate = predicate;
    instr->type = type;
    instr->cycle = cycle;
    instr->done = done;
    return instr;
}

/**
 * @brief Rename the scheduled instructions to eliminate WARs and WAWs.
 * @param state The pointer to the processor state.
 * @param table The pointer to the dependency table.
 */
void registerAllocation(ProcessorState *state, DependencyTable *table)
{
    // Allocate registers to instructions in the VLIW bundle based on the DependencyTable

    int reg = 1;

    // Step 1. allocate registers to the destination registers of all instructions in the VLIW bundles
    for (int i = 0; i < state->bundles.size; i++)
    {
        VLIW *vliw = &state->bundles.vliw[i];
        // printf("dest: %d\n", vliw->alu2->dest);
        //  ALU1
        if (vliw->alu1->type != NOP && vliw->alu1->dest > 0)
        {
            // Allocate registers
            vliw->alu1->dest = reg;
            reg++;
        }
        // ALU2
        if (vliw->alu2->type != NOP && vliw->alu2->dest > 0)
        {
            vliw->alu2->dest = reg;
            reg++;
        }
        // MULT
        if (vliw->mult->type != NOP && vliw->mult->dest > 0)
        {
            vliw->mult->dest = reg;
            reg++;
        }
        // MEM
        if (vliw->mem->type == LD && vliw->mem->dest > 0)
        { // only rename LDs (STs are not renamed)
            vliw->mem->dest = reg;
            reg++;
        }
    }

    //printf("All okay\n");

    // Step 2. change the source registers to the allocated destination registers of the instructions they depend on
    // Iterate over the instructions in the DependencyTable and if they have dependencies
    // in the localDeps, interloopDeps, invariantDeps or postloopDeps,
    // change the source registers to the allocated destination registers of the instructions they depend on
    for (int k = 0; k < table->size; k++)
    {
        DependencyEntry *entry = &table->dependencies[k];

        int reg1 = instrs.instructions[entry->ID - 65].src1; // source register 1
        int reg2 = instrs.instructions[entry->ID - 65].src2; // source register 2

        LatestDependency latestDep = findLatestDependency(table, entry);
        if(entry->ID-65 == 6){
            printf("-->latestDep.idx: %d\n", latestDep.idx);
            printf("-->latestDep.dest: %d\n", latestDep.dest);
            printf("-->latestDep.idxOtherSrc: %d\n", latestDep.idxOtherSrc);
            printf("-->latestDep.destOtherSrc: %d\n", latestDep.destOtherSrc);
            
        }

        
        // interloopDeps
        // only check for interloop dependencies if the latest found dependency producer is NOT in BB0
        // (case where there exist 2 producers, one in BB0 and one in BB1, and the consumer is in BB1 or BB2)    TODO consumer in BB2 ?
        if (!(latestDep.block == 0 && latestDep.dest == reg1))
        { // check interloop for reg1
            //printf("entry->ID-65: %d\n", entry->ID - 65);
            for (int j = 0; j < entry->loop.size; j++)
            {
                char id = entry->loop.list[j].ID;
                char other = entry->ID;

                if (id == other)
                {
                    //printf("skipper:\n");

                    continue;
                }

                if (entry->loop.list[j].reg == reg1)
                {
                    //printf("entry->loop.list[j].ID-65: %d\n", entry->loop.list[j].ID - 65);

                    if (table->dependencies[entry->loop.list[j].ID - 65].type == MULU)
                    {
                        //printf("entry->loop.list[j].ID-65: nonono %d\n", entry->loop.list[j].ID - 65);
                        if ((table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2) > latestDep.scheduledTime)
                        {
                            //printf("into\n");
                            latestDep.scheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2;
                            latestDep.idx = entry->loop.list[j].ID - 65;
                        }
                    }
                    else if (table->dependencies[entry->loop.list[j].ID - 65].scheduledTime > latestDep.scheduledTime)
                    {
                        //printf("entry->loop.list[j].ID-65 bababa: %d\n", entry->loop.list[j].ID - 65);
                        latestDep.scheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime;
                        latestDep.idx = entry->loop.list[j].ID - 65;
                    }
                }
                //printf("entry->loop.list[j].ID-65  ---: %d\n", entry->loop.list[j].ID - 65);
            }
        }

        if (!(latestDep.blockOtherSrc == 0 && latestDep.destOtherSrc == reg2))
        { // check interloop for reg2

            for (int j = 0; j < entry->loop.size; j++)
            {
                if (entry->loop.list[j].reg == reg2)
                {
                    if (table->dependencies[entry->loop.list[j].ID - 65].type == MULU)
                    {
                        if ((table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2) > latestDep.scheduledTimeOtherSrc)
                        {
                            latestDep.scheduledTimeOtherSrc = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2;
                            latestDep.idxOtherSrc = entry->loop.list[j].ID - 65;
                        }
                    }
                    else if (table->dependencies[entry->loop.list[j].ID - 65].scheduledTime > latestDep.scheduledTimeOtherSrc)
                    {
                        latestDep.scheduledTimeOtherSrc = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime;
                        latestDep.idxOtherSrc = entry->loop.list[j].ID - 65;
                    }
                }
            }
        }

        // change the source registers to the allocated destination registers of the instructions they depend on
        if (latestDep.idx != -1)
        {
            if (table->dependencies[latestDep.idx].dest == reg1)
            {
                instrs.instructions[entry->ID - 65].src1 = instrs.instructions[latestDep.idx].dest;
            } else if (table->dependencies[latestDep.idx].dest == reg2)
            {
                instrs.instructions[entry->ID - 65].src2 = instrs.instructions[latestDep.idx].dest;
            }
        }
        if (latestDep.idxOtherSrc != -1)
        {
            if (table->dependencies[latestDep.idxOtherSrc].dest == reg2)
            {
                instrs.instructions[entry->ID - 65].src2 = instrs.instructions[latestDep.idxOtherSrc].dest;
            } else if (table->dependencies[latestDep.idxOtherSrc].dest == reg1)
            {
                instrs.instructions[entry->ID - 65].src1 = instrs.instructions[latestDep.idxOtherSrc].dest;
            }
        }
    }

    // Step 3. fix interloopDeps
    // for every instruction in DependencyTable, check if it has an interloop dependency with itself as a producer
    // if yes, schedule a MOV instruction to copy the value of the source register to the destination register
    for (int j = 0; j < table->size; j++)
    {
        DependencyEntry *entry = &table->dependencies[j];
        // check if it has a dependency with itself as a producer
        for (int k = 0; k < entry->loop.size; k++)
        {
            if (entry->loop.list[k].ID == entry->ID)
            {
                int reg = entry->loop.list[k].src ==1 ? instrs.instructions[entry->ID - 65].src1 : instrs.instructions[entry->ID - 65].src2;
                // find the last time that the same register was used as a source register
                int lastUsedTime = -1;
                int idxLastUsed = -1;
                int regDep = -1;
                InstructionType type = NOP;
                for (int l = instrs.loopStart; l < instrs.loopEnd + 1; l++)
                {
                    if (instrs.instructions[l].src1 == reg || instrs.instructions[l].src2 == reg)
                    {
                        if (table->dependencies[l].scheduledTime > lastUsedTime)
                        {
                            type = instrs.instructions[l].src1 == reg ? instrs.instructions[l].type : instrs.instructions[l].type;
                            lastUsedTime = table->dependencies[l].scheduledTime;
                            idxLastUsed = l;
                            regDep = instrs.instructions[l].src1 == reg ? instrs.instructions[l].src1 : instrs.instructions[l].src2;
                            printf("regDep: %d\n", regDep);
                        }
                    }
                }
                printf("type of last used: %d\n", type);

                if (type == MULU)
                {
                    lastUsedTime += 3;
                }

                // schedule a MOV instruction to copy the value of the source register to the destination register
                // in the VLIW bundle with the latest scheduled instruction if one of the two ALUs is free
                VLIW *vliw = &(state->bundles.vliw[lastUsedTime]);
                

                printf("lastUsedTime: %d\n", lastUsedTime);

                

                if (vliw->alu1->type == NOP)
                {
                    //printf("ALU1\n");
                    vliw->alu1 = createNewInstruction("mov", regDep, instrs.instructions[entry->ID - 65].dest,-1, 0, 0, MOV, 0, false);
                }
                else if (vliw->alu2->type == NOP)
                {
                    vliw->alu2 = createNewInstruction("mov", regDep, instrs.instructions[entry->ID - 65].dest,-1, 0, 0, MOV, 0, false);
                }
                else
                {
                    //printf("instruction: %s\n", instrs.instructions[10].opcode);

                    // create a new VLIW bundle
                    state->bundles.size += 1;
                    state->bundles.vliw = (VLIW *)realloc(state->bundles.vliw, state->bundles.size * sizeof(VLIW));
                    state->bundles.vliw[state->bundles.size - 1].alu1 = &NOPinstr;
                    state->bundles.vliw[state->bundles.size - 1].alu2 = &NOPinstr;
                    state->bundles.vliw[state->bundles.size - 1].mult = &NOPinstr;
                    state->bundles.vliw[state->bundles.size - 1].mem = &NOPinstr;
                    state->bundles.vliw[state->bundles.size - 1].br = &NOPinstr;

                    VLIW *vliw2 = &state->bundles.vliw[state->bundles.size - 1];
                    vliw2->alu1 = createNewInstruction("mov", regDep, instrs.instructions[entry->ID - 65].dest,-1, 0, 0, MOV, 0, false);
                    // move the loop instruction to the new VLIW bundle
                    vliw2->br = vliw->br;
                    vliw->br = &NOPinstr;
                }
            }
        }
    }

    // Step 4. allocate unused register for all source registers that have no RAW dependency
    // for every instruction in DependencyTable, check if it has a dependency on another instruction
    // if not, allocate a register to the source register
    for (int j = 0; j < table->size; j++)
    {
        DependencyEntry *entry = &table->dependencies[j];
    
        if (entry->local.size == 0 && entry->loop.size == 0 && entry->invariant.size == 0 && entry->postL.size == 0)
        {
            // allocate a register to the source register
            if (instrs.instructions[entry->ID - 65].src1 != -1)
            {
                instrs.instructions[entry->ID - 65].src1 = reg;
                reg++;
            }

            if (instrs.instructions[entry->ID - 65].src2 != -1)
            {
                instrs.instructions[entry->ID - 65].src2 = reg;
                reg++;
            }
        }
    }

    state->bundles.vliw[table->dependencies[instrs.loopEnd].scheduledTime].br->imm = table->dependencies[instrs.loopStart].scheduledTime;
}

/**
 * @brief Rename the scheduled instructions to eliminate WARs and WAWs when LOOP_PIP.
 * @param state The pointer to the processor state.
 * @param table The pointer to the dependency table.
 */
void registerAllocationPip(ProcessorState *state, DependencyTable *table)
{
    // Allocate registers to instructions in the VLIW bundle based on the DependencyTable

    int offset = 1; // offset for rotating registers
    int reg = 1;    // for non-rotating registers

    // TODO separate state in 1 VLIW bundles for BB0, 1 for BB1 and 1 for BB2 ?

    // Step 1. allocate ROTATING registers to the destination registers of all instructions in the VLIW bundles
    for (int i = table->dependencies[instrs.loopStart].scheduledTime; i < table->dependencies[instrs.loopEnd].scheduledTime + 1; i++)
    {
        VLIW *vliw = &state->bundles.vliw[i];
        // ALU1
        if (vliw->alu1->type != NOP)
        {
            // Allocate registers
            vliw->alu1->dest = offset + ROTATION_START_INDEX + state->RRB;
            offset++;
            offset += state->stage;
        }
        // ALU2
        if (vliw->alu2->type != NOP)
        {
            vliw->alu2->dest = offset + ROTATION_START_INDEX + state->RRB;
            offset++;
            offset += state->stage;
        }
        // MULT
        if (vliw->mult->type != NOP)
        {
            vliw->mult->dest = offset + ROTATION_START_INDEX + state->RRB;
            offset++;
            offset += state->stage;
        }
        // MEM
        if (vliw->mem->type == LD)
        { // only rename LDs (STs are not renamed)
            vliw->mem->dest = offset + ROTATION_START_INDEX + state->RRB;
            offset++;
            offset += state->stage;
        }
    }

    // Step 3. for local and interloop dependencies, adjust the src registers by adding the difference
    // of the stage of the consumer and the stage of the producer (+1 for interloop)
    // (a new stage every II cycles, a new loop => new stage too)

    for (int j = instrs.loopStart; j < instrs.loopEnd + 1; j++)
    {
        DependencyEntry *entry = &table->dependencies[j];

        // find the latest dependency for src1 and src2
        int reg1 = instrs.instructions[entry->ID - 65].src1; // source register 1
        int reg2 = instrs.instructions[entry->ID - 65].src2; // source register 2

        int latestDep1 = -1;
        int latestDep2 = -1;

        int stageDiff1 = floor(table->dependencies[entry->ID - 65].scheduledTime / state->II);
        int stageDiff2 = floor(table->dependencies[entry->ID - 65].scheduledTime / state->II);

        for (int k = 0; k < entry->local.size; k++)
        {
            // MUST use instrs.instructions[entry->local.list[j].ID-65].dest
            // bc table->dependencies[entry->local.list[j].ID-65].dest NOT renamed
            if (entry->local.list[j].reg == reg1 && table->dependencies[entry->local.list[j].ID - 65].scheduledTime > latestDep1)
            {
                int stageDiff = floor(table->dependencies[entry->local.list[j].ID - 65].scheduledTime / state->II);

                instrs.instructions[entry->ID - 65].src1 = instrs.instructions[entry->local.list[j].ID - 65].dest + (stageDiff1 - stageDiff);
                latestDep1 = table->dependencies[entry->local.list[j].ID - 65].scheduledTime;
            }
            if (entry->local.list[j].reg == reg2 && table->dependencies[entry->local.list[j].ID - 65].scheduledTime > latestDep2)
            {
                int stageDiff = floor(table->dependencies[entry->local.list[j].ID - 65].scheduledTime / state->II);

                instrs.instructions[entry->ID - 65].src2 = instrs.instructions[entry->local.list[j].ID - 65].dest + (stageDiff2 - stageDiff);
                latestDep2 = table->dependencies[entry->local.list[j].ID - 65].scheduledTime;
            }
        }

        // reset
        latestDep1 = -1;
        latestDep2 = -1;

        for (int k = 0; k < entry->loop.size; k++)
        {
            if (entry->loop.list[j].reg == reg1 && table->dependencies[entry->loop.list[j].ID - 65].scheduledTime > latestDep1)
            {
                int stageDiff = floor(table->dependencies[entry->loop.list[j].ID - 65].scheduledTime / state->II);

                instrs.instructions[entry->ID - 65].src1 = instrs.instructions[entry->loop.list[j].ID - 65].dest + (stageDiff1 - stageDiff) + 1;
                latestDep1 = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime;
            }
            if (entry->loop.list[j].reg == reg2 && table->dependencies[entry->loop.list[j].ID - 65].scheduledTime > latestDep2)
            {
                int stageDiff = floor(table->dependencies[entry->loop.list[j].ID - 65].scheduledTime / state->II);

                instrs.instructions[entry->ID - 65].src2 = instrs.instructions[entry->loop.list[j].ID - 65].dest + (stageDiff2 - stageDiff) + 1;
                latestDep2 = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime;
            }
        }

        // TODO what if both local and interloop ?
    }

    // Step 0. destination register renaming and local dependency renaming for BB0 and BB2
    // rename the destination registers in BB0
    for (int i = 0; i < table->dependencies[instrs.loopStart].scheduledTime; i++)
    {
        VLIW *vliw = &state->bundles.vliw[i];
        // ALU1
        if (vliw->alu1->type != NOP)
        {
            // Allocate registers
            bool dependance = false;
            for (int j = instrs.loopStart; j < instrs.loopEnd + 1; j++)
            {
                DependencyEntry *entry = &table->dependencies[j];
                for (int k = 0; k < entry->loop.size; k++)
                {
                    if (entry->local.list[j].reg == vliw->alu1->dest)
                    {
                        int stageDiff = floor(table->dependencies[entry->loop.list[j].ID - 65].scheduledTime / state->II);
                        vliw->alu1->dest = instrs.instructions[entry->loop.list[j].ID - 65].dest - stageDiff + 1; // -S(P), iteration = 1
                        dependance = true;
                    }
                }
            }
            if (!dependance)
            {
                vliw->alu1->dest = reg;
                reg++;
            }
        }
        // ALU2
        if (vliw->alu2->type != NOP)
        {
            bool dependance = false;
            for (int j = instrs.loopStart; j < instrs.loopEnd + 1; j++)
            {
                DependencyEntry *entry = &table->dependencies[j];
                for (int k = 0; k < entry->loop.size; k++)
                {
                    if (entry->local.list[j].reg == vliw->alu2->dest)
                    {
                        int stageDiff = floor(table->dependencies[entry->loop.list[j].ID - 65].scheduledTime / state->II);
                        vliw->alu2->dest = instrs.instructions[entry->loop.list[j].ID - 65].dest - stageDiff + 1; // -S(P), iteration = 1
                        dependance = true;
                    }
                }
            }
            if (!dependance)
            {
                vliw->alu2->dest = reg;
                reg++;
            }
        }
        // MULT
        if (vliw->mult->type != NOP)
        {
            bool dependance = false;
            for (int j = instrs.loopStart; j < instrs.loopEnd + 1; j++)
            {
                DependencyEntry *entry = &table->dependencies[j];
                for (int k = 0; k < entry->loop.size; k++)
                {
                    if (entry->local.list[j].reg == vliw->mult->dest)
                    {
                        int stageDiff = floor(table->dependencies[entry->loop.list[j].ID - 65].scheduledTime / state->II);
                        vliw->mult->dest = instrs.instructions[entry->loop.list[j].ID - 65].dest - stageDiff + 1; // -S(P), iteration = 1
                        dependance = true;
                    }
                }
            }
            if (!dependance)
            {
                vliw->mult->dest = reg;
                reg++;
            }
        }
        // MEM
        if (vliw->mem->type == LD)
        { // only rename LDs (STs are not renamed)
            bool dependance = false;
            for (int j = instrs.loopStart; j < instrs.loopEnd + 1; j++)
            {
                DependencyEntry *entry = &table->dependencies[j];
                for (int k = 0; k < entry->loop.size; k++)
                {
                    if (entry->local.list[j].reg == vliw->mem->dest)
                    {
                        int stageDiff = floor(table->dependencies[entry->loop.list[j].ID - 65].scheduledTime / state->II);
                        vliw->mem->dest = instrs.instructions[entry->loop.list[j].ID - 65].dest - stageDiff + 1; // -S(P), iteration = 1
                        dependance = true;
                    }
                }
            }
            if (!dependance)
            {
                vliw->mem->dest = reg;
                reg++;
            }
        }
    }

    // BB2
    for (int i = table->dependencies[instrs.loopEnd].scheduledTime + 1; i < state->bundles.size; i++)
    {
        VLIW *vliw = &state->bundles.vliw[i];
        // ALU1
        if (vliw->alu1->type != NOP)
        {
            // Allocate registers
            vliw->alu1->dest = reg;
            reg++;
        }
        // ALU2
        if (vliw->alu2->type != NOP)
        {
            vliw->alu2->dest = reg;
            reg++;
        }
        // MULT
        if (vliw->mult->type != NOP)
        {
            vliw->mult->dest = reg;
            reg++;
        }
        // MEM
        if (vliw->mem->type == LD)
        { // only rename LDs (STs are not renamed)
            vliw->mem->dest = reg;
            reg++;
        }
    }

    // rename the source registers for local dependencies in BB0
    for (int i = 0; i < instrs.loopStart; i++)
    {
        DependencyEntry *entry = &table->dependencies[i];
        if (entry->local.size != 0)
        {
            // find the latest dependency for src1 and src2
            int reg1 = instrs.instructions[entry->ID - 65].src1; // source register 1
            int reg2 = instrs.instructions[entry->ID - 65].src2; // source register 2

            int latestDep1 = -1;
            int latestDep2 = -1;

            for (int j = 0; j < entry->local.size; j++)
            {
                if (entry->local.list[j].reg == reg1 && table->dependencies[entry->local.list[j].ID - 65].scheduledTime > latestDep1)
                {
                    instrs.instructions[entry->ID - 65].src1 = table->dependencies[entry->local.list[j].ID - 65].dest;
                    latestDep1 = table->dependencies[entry->local.list[j].ID - 65].scheduledTime;
                }
                if (entry->local.list[j].reg == reg2 && table->dependencies[entry->local.list[j].ID - 65].scheduledTime > latestDep2)
                {
                    instrs.instructions[entry->ID - 65].src2 = table->dependencies[entry->local.list[j].ID - 65].dest;
                    latestDep2 = table->dependencies[entry->local.list[j].ID - 65].scheduledTime;
                }
            }
        }
    }

    // rename the source registers for local dependencies in BB2
    for (int i = instrs.loopEnd + 1; i < table->size; i++)
    {
        DependencyEntry *entry = &table->dependencies[i];
        if (entry->local.size != 0)
        {
            // find the latest dependency for src1 and src2
            int reg1 = instrs.instructions[entry->ID - 65].src1; // source register 1
            int reg2 = instrs.instructions[entry->ID - 65].src2; // source register 2

            int latestDep1 = -1;
            int latestDep2 = -1;

            for (int j = 0; j < entry->local.size; j++)
            {
                if (entry->local.list[j].reg == reg1 && table->dependencies[entry->local.list[j].ID - 65].scheduledTime > latestDep1)
                {
                    instrs.instructions[entry->ID - 65].src1 = table->dependencies[entry->local.list[j].ID - 65].dest;
                    latestDep1 = table->dependencies[entry->local.list[j].ID - 65].scheduledTime;
                }
                if (entry->local.list[j].reg == reg2 && table->dependencies[entry->local.list[j].ID - 65].scheduledTime > latestDep2)
                {
                    instrs.instructions[entry->ID - 65].src2 = table->dependencies[entry->local.list[j].ID - 65].dest;
                    latestDep2 = table->dependencies[entry->local.list[j].ID - 65].scheduledTime;
                }
            }
        }
    }

    // Step 2. allocate non-rotating register to the invariant dependencies
    for (int j = instrs.loopStart; j < instrs.loopEnd + 1; j++)
    {
        DependencyEntry *entry = &table->dependencies[j];
        if (entry->invariant.size != 0)
        {
            // find the latest dependency for src1 and src2
            int reg1 = instrs.instructions[entry->ID - 65].src1; // source register 1
            int reg2 = instrs.instructions[entry->ID - 65].src2; // source register 2

            int latestDep1 = -1;
            int latestDep2 = -1;

            for (int k = 0; k < entry->invariant.size; k++)
            {
                if (entry->invariant.list[j].reg == reg1 && table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime > latestDep1)
                {
                    instrs.instructions[entry->ID - 65].src1 = table->dependencies[entry->invariant.list[j].ID - 65].dest;
                    latestDep1 = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime;
                }
                if (entry->invariant.list[j].reg == reg2 && table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime > latestDep2)
                {
                    instrs.instructions[entry->ID - 65].src2 = table->dependencies[entry->invariant.list[j].ID - 65].dest;
                    latestDep2 = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime;
                }
            }
        }
    }

    // Step 4. for postloop dependencies, adjust the src registers by adding the difference
    // considering the BB2 stage as being the last stage of the loop

    // TODO (!) check the -1  used to assign last stage to the post loop dependencies
    // same for all postloop
    int stageDiffPost = floor((table->dependencies[instrs.loopEnd + 1].scheduledTime - 1) / state->II);

    for (int j = instrs.loopEnd + 1; j < table->size; j++)
    {
        DependencyEntry *entry = &table->dependencies[j];

        // find the latest dependency for src1 and src2
        int reg1 = instrs.instructions[entry->ID - 65].src1; // source register 1
        int reg2 = instrs.instructions[entry->ID - 65].src2; // source register 2

        int latestDep1 = -1;
        int latestDep2 = -1;

        for (int k = 0; k < entry->postL.size; k++)
        {
            // MUST use instrs.instructions[entry->postL.list[j].ID-65].dest
            // bc table->dependencies[entry->postL.list[j].ID-65].dest NOT renamed
            if (entry->postL.list[j].reg == reg1 && table->dependencies[entry->postL.list[j].ID - 65].scheduledTime > latestDep1)
            {
                int stageDiff = floor(table->dependencies[entry->postL.list[j].ID - 65].scheduledTime / state->II);

                instrs.instructions[entry->ID - 65].src1 = instrs.instructions[entry->postL.list[j].ID - 65].dest + (stageDiffPost - stageDiff);
                latestDep1 = table->dependencies[entry->postL.list[j].ID - 65].scheduledTime;
            }
            if (entry->postL.list[j].reg == reg2 && table->dependencies[entry->postL.list[j].ID - 65].scheduledTime > latestDep2)
            {
                int stageDiff = floor(table->dependencies[entry->postL.list[j].ID - 65].scheduledTime / state->II);

                instrs.instructions[entry->ID - 65].src2 = instrs.instructions[entry->postL.list[j].ID - 65].dest + (stageDiffPost - stageDiff);
                latestDep2 = table->dependencies[entry->postL.list[j].ID - 65].scheduledTime;
            }
        }

        // loop invariant dependencies BB2->BB0

        // reset
        latestDep1 = -1;
        latestDep2 = -1;

        for (int k = 0; k < entry->invariant.size; k++)
        {
            if (entry->invariant.list[j].reg == reg1 && table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime > latestDep1)
            {
                instrs.instructions[entry->ID - 65].src1 = instrs.instructions[entry->invariant.list[j].ID - 65].dest;
                latestDep1 = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime;
            }
            if (entry->invariant.list[j].reg == reg2 && table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime > latestDep2)
            {
                instrs.instructions[entry->ID - 65].src2 = instrs.instructions[entry->invariant.list[j].ID - 65].dest;
                latestDep2 = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime;
            }
        }
    }

    // Step 5. allocate unused register for all source registers that have no RAW dependency
    // for every instruction in DependencyTable, check if it has a dependency on another instruction
    // if not, allocate a register to the source register
    for (int j = 0; j < table->size; j++)
    {
        DependencyEntry *entry = &table->dependencies[j];
        if (entry->local.size == 0 && entry->loop.size == 0 && entry->invariant.size == 0 && entry->postL.size == 0)
        {
            // allocate a register to the source register
            if (instrs.instructions[entry->ID - 65].src1 != -1)
            {
                instrs.instructions[entry->ID - 65].src1 = reg;
                reg++;
            }

            if (instrs.instructions[entry->ID - 65].src2 != -1)
            {
                instrs.instructions[entry->ID - 65].src2 = reg;
                reg++;
            }
        }
    }
}

/**
 * @brief Create a new VLIW bundle.
 * @param state The pointer to the processor state.
 * @return VLIW The new VLIW bundle.
 */
VLIW newVLIW(ProcessorState *state)
{
    // realloc bundles
    state->bundles.size += 1;
    state->bundles.vliw = (VLIW *)realloc(state->bundles.vliw, state->bundles.size * sizeof(VLIW));
    state->bundles.vliw[state->bundles.size - 1].alu1 = &NOPinstr;
    state->bundles.vliw[state->bundles.size - 1].alu2 = &NOPinstr;
    state->bundles.vliw[state->bundles.size - 1].mult = &NOPinstr;
    state->bundles.vliw[state->bundles.size - 1].mem = &NOPinstr;
    state->bundles.vliw[state->bundles.size - 1].br = &NOPinstr;

    return state->bundles.vliw[state->bundles.size - 1];
}

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
void scheduleInstruction(ProcessorState *state, DependencyEntry *entry, SchedulerState *schedulerState)
{
    bool scheduled = false;

    VLIW *vliw = &state->bundles.vliw[state->bundles.size - 1];

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

    if (entry->type == ADD || entry->type == ADDI || entry->type == SUB || entry->type == MOV)
    {
        //printf("Scheduling instruction %c\n", entry->ID);
        while (schedulerState->latestALU1 < state->bundles.size || schedulerState->latestALU2 < state->bundles.size)
        { // latestALU1 == latestALU2 for dependencies
            //printf("latestALU1 = %d, latestALU2 = %d\n", schedulerState->latestALU1, schedulerState->latestALU2);
            if (schedulerState->latestALU1 < state->bundles.size && schedulerState->latestALU1 <= schedulerState->latestALU2)
            { // ALU1 only chosen if it's not later than a free ALU2
                //printf("latestALU1 = %d\n", schedulerState->latestALU1);

                //printf("vliw->alu1->type = %d\n", vliw->alu1->type);
                if (vliw->alu1->type == NOP)
                {
                    //printf("Scheduling instruction %c in ALU1\n", entry->ID);
                    vliw = &state->bundles.vliw[schedulerState->latestALU1]; // take the vliw bundle at the latestALU1 index
                    vliw->alu1 = &instrs.instructions[entry->ID - 65];
                    entry->scheduledTime = schedulerState->latestALU1;
                    //printf("Scheduled time = %d\n", entry->scheduledTime);
                    schedulerState->latestALU1 += 1;

                    scheduled = true;
                    break;
                }
                else
                {
                    schedulerState->latestALU1 += 1;
                }
            }
            else if (schedulerState->latestALU2 < state->bundles.size)
            {
                if (vliw->alu2->type == NOP)
                {
                    vliw = &state->bundles.vliw[schedulerState->latestALU2];
                    vliw->alu2 = &instrs.instructions[entry->ID - 65];
                    entry->scheduledTime = schedulerState->latestALU2;
                    //printf("Scheduled time = %d\n", entry->scheduledTime);
                    schedulerState->latestALU2 += 1;

                    scheduled = true;
                    break;
                }
                else
                {
                    schedulerState->latestALU2 += 1;
                }
            }
        }
        if (!scheduled)
        {
            //printf("Creating new VLIW bundle\n");
            newVLIW(state);
            vliw = &(state->bundles.vliw[state->bundles.size - 1]);
            vliw->alu1 = &instrs.instructions[entry->ID - 65];
            entry->scheduledTime = schedulerState->latestALU1; // latestALU1 will be == the size of the bundles-1
            schedulerState->latestALU1 += 1;
        }
    }
    else if (entry->type == MULU)
    {
        //printf("Scheduler state latestMult = %d\n", schedulerState->latestMult);
        while (schedulerState->latestMult < state->bundles.size)
        {
            //printf("------>\n");
            if (vliw->mult->type == NOP)
            {
                //printf("Scheduler state latestMult 2 = %d\n", schedulerState->latestMult);
                vliw = &state->bundles.vliw[schedulerState->latestMult];
                vliw->mult = &instrs.instructions[entry->ID - 65];
                entry->scheduledTime = schedulerState->latestMult;
                //printf("Scheduled time = %d\n", entry->scheduledTime);
                schedulerState->latestMult += 1;

                scheduled = true;
                break;
            }
            schedulerState->latestMult += 1;
        }
        if (!scheduled)
        {
            newVLIW(state);
            vliw = &state->bundles.vliw[state->bundles.size - 1];
            vliw->mult = &instrs.instructions[entry->ID - 65];
            entry->scheduledTime = schedulerState->latestMult;
            schedulerState->latestMult += 1;
        }
    }
    else if (entry->type == LD || entry->type == ST)
    {
        while (schedulerState->latestMem < state->bundles.size)
        {
            if (vliw->mem->type == NOP)
            {
                vliw = &state->bundles.vliw[schedulerState->latestMem];
                vliw->mem = &instrs.instructions[entry->ID - 65];
                entry->scheduledTime = schedulerState->latestMem;
                schedulerState->latestMem += 1;

                scheduled = true;
                break;
            }
            schedulerState->latestMem += 1;
        }
        if (!scheduled)
        {
            newVLIW(state);
            vliw = &state->bundles.vliw[state->bundles.size - 1];
            vliw->mem = &instrs.instructions[entry->ID - 65];
            entry->scheduledTime = schedulerState->latestMem;
            schedulerState->latestMem += 1;
        }
    }

    // update the latest loop instruction to make sure the branch is NOT placed earlier than any other instruction in the loop
    if (instrs.instructions[entry->ID - 65].block == 1 && entry->type != LOOP && entry->type != LOOP_PIP)
    {
        schedulerState->latestBr = max(schedulerState->latestBr, entry->scheduledTime);
    }

    // BR instruction must always be scheduled in LAST bundle of BB1
    else if (entry->type == LOOP || entry->type == LOOP_PIP)
    {
        while (schedulerState->latestBr < state->bundles.size)
        {
            if (vliw->br->type == NOP)
            { // probably not necessary
                vliw = &state->bundles.vliw[schedulerState->latestBr];
                vliw->br = &instrs.instructions[entry->ID - 65];
                entry->scheduledTime = schedulerState->latestBr;
                schedulerState->latestBr += 1;

                scheduled = true;
                break;
            }
            schedulerState->latestBr += 1;
        }
        if (!scheduled)
        {
            newVLIW(state);
            vliw = &state->bundles.vliw[state->bundles.size - 1];
            vliw->br = &instrs.instructions[entry->ID - 65];
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
void scheduleInstructions(ProcessorState *state, DependencyTable *table)
{
    // Schedule instructions in the VLIW bundle based on the DependencyTable
    // printf("bundle size = %d\n", state->bundles.size);

    SchedulerState schedulerState;
    schedulerState.latestALU1 = 0;
    schedulerState.latestALU2 = 0;
    schedulerState.latestMult = 0;
    schedulerState.latestMem = 0;
    schedulerState.latestBr = 0;

    // create a new VLIW bundle
    VLIW v = newVLIW(state);
    VLIW *vliw = &v;

    for (int i = 0; i < table->size; i++)
    {
        // printf("ID: %d\n", i);
        DependencyEntry *entry = &table->dependencies[i];

        // When enter BB1 or BB2, stop using the previous VLIW bundles and start a new one
        // and set the latest counters to the size of the bundles-1
        if ((instrs.instructions[i].block == 1 && instrs.instructions[i - 1].block == 0) || // first instruction in BB1
            (instrs.instructions[i].block == 2 && instrs.instructions[i - 1].block == 1))
        {

            // first instruction in BB2
            newVLIW(state);
            vliw = &state->bundles.vliw[state->bundles.size - 1];
            schedulerState.latestALU1 = state->bundles.size - 1;
            schedulerState.latestALU2 = state->bundles.size - 1;
            schedulerState.latestMult = state->bundles.size - 1;
            schedulerState.latestMem = state->bundles.size - 1;
            schedulerState.latestBr = state->bundles.size - 1;
        }
        // printf("bundle size = %d\n", state->bundles.size);

        // If there are no local, invariant or post loop dependencies (CAN have interloop dependencies !)
        if (entry->local.size == 0 && entry->invariant.size == 0 && entry->postL.size == 0)
        {
            // printf("no local, invariant or post loop\n");
            fflush(stdout);
            // Schedule the instruction in the VLIW bundle

            // updated list of latest used ALU1, ALU2, MULT, MEM, BR, and loop instruction indices
            scheduleInstruction(state, entry, &schedulerState);
        }
        //-->   // Local, invariant or post loop dependencies (at least 1)
        else
        {
            // check the LATEST schedule time of the instruction(s) it depends on
            LatestDependency latestDep = findLatestDependency(table, entry);
            printf("Instruction %c\n", entry->ID);
            printf("latestDep.idx = %d\n", latestDep.idx);
            printf("latestDep.scheduledTime = %d\n", latestDep.scheduledTime);
            printf("latestDep2 idx = %d\n", latestDep.idxOtherSrc);

            int latestScheduledTime = latestDep.scheduledTime;
            int IDdependsOn = latestDep.idx;

            // scheduling cases for different instruction types
            printf("-->type of latest %d\n", table->dependencies[IDdependsOn].type);
            // schedule the instruction in the first VLIW bundle with the available correct unit and the latest schedule time
            if (table->dependencies[IDdependsOn].type == MULU)
            {

                if (entry->type == ADD || entry->type == ADDI || entry->type == SUB || entry->type == MOV)
                {
                    int oldLatest = schedulerState.latestALU1;
                    int oldLatest2 = schedulerState.latestALU2;
                    schedulerState.latestALU1 = max(oldLatest, latestScheduledTime + 1);
                    schedulerState.latestALU2 = max(oldLatest2, latestScheduledTime + 1);

                    int minALUTime = (schedulerState.latestALU1 < schedulerState.latestALU2) ? schedulerState.latestALU1 : schedulerState.latestALU2;

                    for (int j = oldLatest; j < minALUTime; j++)
                    {
                        newVLIW(state);
                        vliw = &state->bundles.vliw[state->bundles.size - 1];
                    }
                    // schedule the instruction in the VLIW bundle with the latest scheduled time
                    // vliw->mult = &instrs.instructions[entry->ID-65];
                    // entry->scheduledTime = schedulerState.latestMult;

                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestALU1 = oldLatest;
                    schedulerState.latestALU2 = oldLatest2;
                }
                else if (entry->type == LD || entry->type == ST)
                {
                    int oldLatest = schedulerState.latestMem;
                    schedulerState.latestMem = max(oldLatest, latestScheduledTime + 1);
                    for (int j = oldLatest; j < schedulerState.latestMem; j++)
                    {
                        newVLIW(state);
                        vliw = &state->bundles.vliw[state->bundles.size - 1];
                    }
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestMem = oldLatest;
                }
                else if (entry->type == MULU)
                {
                    int oldLatest = schedulerState.latestMult;
                    schedulerState.latestMult = max(oldLatest, latestScheduledTime + 1);
                    for (int j = oldLatest; j < schedulerState.latestMult; j++)
                    {
                        newVLIW(state);
                        vliw = &state->bundles.vliw[state->bundles.size - 1];
                    }
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestMult = oldLatest;
                }

                // new entry must be scheduled AFTER its dependency is resolved (latestScheduledTime + latency) or later
                // only need to do +1 because +2 was already done when computing latestScheduledTime
                // create as many new VLIW bundles as the difference between the old latest and the new latest
            }
            else if (table->dependencies[IDdependsOn].type == LD || table->dependencies[IDdependsOn].type == ST)
            {
                printf("------->LD or ST\n");

                if (entry->type == ADD || entry->type == ADDI || entry->type == SUB || entry->type == MOV)
                {
                    int oldLatest = schedulerState.latestALU1;
                    int oldLatest2 = schedulerState.latestALU2;
                    schedulerState.latestALU1 = max(oldLatest, latestScheduledTime + 1);
                    schedulerState.latestALU2 = max(oldLatest2, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestALU1 = oldLatest;
                    schedulerState.latestALU2 = oldLatest2;
                }
                else if (entry->type == LD || entry->type == ST)
                {
                    int oldLatest = schedulerState.latestMem;
                    schedulerState.latestMem = max(oldLatest, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestMem = oldLatest;
                }
                else if (entry->type == MULU)
                {
                    int oldLatest = schedulerState.latestMult;
                    schedulerState.latestMult = max(oldLatest, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestMult = oldLatest;
                }
            }
            else
            {
                if (entry->type == ADD || entry->type == ADDI || entry->type == SUB || entry->type == MOV)
                {
                    int oldLatest = schedulerState.latestALU1;
                    int oldLatest2 = schedulerState.latestALU2;
                    schedulerState.latestALU1 = max(oldLatest, latestScheduledTime + 1);
                    schedulerState.latestALU2 = max(oldLatest2, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestALU1 = oldLatest;
                    schedulerState.latestALU2 = oldLatest2;
                }
                else if (entry->type == LD || entry->type == ST)
                {
                    int oldLatest = schedulerState.latestMem;
                    schedulerState.latestMem = max(oldLatest, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestMem = oldLatest;
                }
                else if (entry->type == MULU)
                {
                    int oldLatest = schedulerState.latestMult;
                    schedulerState.latestMult = max(oldLatest, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestMult = oldLatest;
                }
            }
        }

        // if there is a loop
        if (instrs.loopStart != -1)
        {
            //-->       // Interloop dependencies
            // handled right after all of BB1 is scheduled
            if (table->dependencies[instrs.loopEnd].scheduledTime != -1 && table->dependencies[instrs.loopEnd + 1].scheduledTime == -1)
            {
                for (int i = instrs.loopStart; i < instrs.loopEnd + 1; i++)
                {
                    DependencyEntry *entry = &table->dependencies[i];

                    if (entry->loop.size != 0)
                    {
                        int latestScheduledTime = -1;
                        int IDdependsOn = -1;
                        // look for the latest scheduled time of the instruction(s) it depends on ONLY in the loop (i.e. block == 1)
                        for (int j = 0; j < entry->loop.size; j++)
                        {
                            if (table->dependencies[entry->loop.list[j].ID - 65].type == MULU)
                            {
                                if ((table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2) > latestScheduledTime)
                                {
                                    latestScheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2;
                                    IDdependsOn = entry->loop.list[j].ID - 65;
                                }
                            }
                            else if (table->dependencies[entry->loop.list[j].ID - 65].scheduledTime > latestScheduledTime)
                            {
                                latestScheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime;
                                IDdependsOn = entry->loop.list[j].ID - 65;
                            }
                        }

                        // only case that needs to be handled: depends on a MULU
                        if (table->dependencies[IDdependsOn].type == MULU)
                        {
                            // check that the distance between this instruction and the entry is at least 3 cycles

                            // distance between the beginning of the loop and the instruction scheduled time
                            int x = entry->scheduledTime - instrs.loopStart;
                            // distance between the end of the loop scheduled time and the instruction it depends on
                            int y = table->dependencies[instrs.loopEnd].scheduledTime - table->dependencies[IDdependsOn].scheduledTime;

                            // if the distance is less than 3, need to add NOP cycles
                            for (int j = 0; j < (3 - (x + y + 1)); j++)
                            {
                                VLIW v = newVLIW(state);
                                vliw = &v;
                            }
                            // postpone the LOOP or LOOP_PIP instruction to the end of the loop
                            vliw = &state->bundles.vliw[table->dependencies[instrs.loopEnd].scheduledTime];
                            vliw->br = &NOPinstr;

                            vliw = &state->bundles.vliw[state->bundles.size - 1];
                            vliw->br = &instrs.instructions[instrs.loopEnd];

                            schedulerState.latestBr = state->bundles.size - 1;
                            table->dependencies[instrs.loopEnd].scheduledTime = schedulerState.latestBr;
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief Schedule instructions in the VLIW bundles based on the DependencyTable using LOOP_PIP.
 * @param state Pointer to the processor state.
 * @param table Pointer to the DependencyTable.
 */
void scheduleInstructionsPip(ProcessorState *state, DependencyTable *table)
{
    // Schedule instructions in the VLIW bundle based on the DependencyTable

    // compute IIRes
    state->II = calculateIIRes(state);

    SchedulerState schedulerState;
    schedulerState.latestALU1 = 0;
    schedulerState.latestALU2 = 0;
    schedulerState.latestMult = 0;
    schedulerState.latestMem = 0;
    schedulerState.latestBr = 0;

    // create a new VLIW bundle
    VLIW v = newVLIW(state);
    VLIW *vliw = &v;

    for (int i = 0; i < table->size; i++)
    {
        DependencyEntry *entry = &table->dependencies[i];

        // When enter BB1 or BB2, stop using the previous VLIW bundles and start a new one
        // and set the latest counters to the size of the bundles-1
        if ((instrs.instructions[i].block == 1 && instrs.instructions[i - 1].block == 0) || // first instruction in BB1
            (instrs.instructions[i].block == 2 && instrs.instructions[i - 1].block == 1))
        { // first instruction in BB2
            VLIW v = newVLIW(state);
            vliw = &v;
            schedulerState.latestALU1 = state->bundles.size - 1;
            schedulerState.latestALU2 = state->bundles.size - 1;
            schedulerState.latestMult = state->bundles.size - 1;
            schedulerState.latestMem = state->bundles.size - 1;
            schedulerState.latestBr = state->bundles.size - 1;
        }

        // If there are no local, invariant or post loop dependencies (CAN have interloop dependencies !)
        if (entry->local.size == 0 && entry->invariant.size == 0 && entry->postL.size == 0)
        {
            // Schedule the instruction in the VLIW bundle

            // updated list of latest used ALU1, ALU2, MULT, MEM, BR, and loop instruction indices
            scheduleInstruction(state, entry, &schedulerState);
        }

        //-->   // Local, invariant or post loop dependencies (at least 1)
        else
        {
            // check the LATEST schedule time of the instruction(s) it depends on
            LatestDependency latestDep = findLatestDependency(table, entry);

            int latestScheduledTime = latestDep.scheduledTime;
            int IDdependsOn = latestDep.idx;

            // scheduling cases for different instruction types

            // schedule the instruction in the first VLIW bundle with the available correct unit and the latest schedule time
            if (table->dependencies[IDdependsOn].type == MULU)
            {
                int oldLatest = schedulerState.latestMult; // save copy of old latest
                // new entry must be scheduled AFTER its dependency is resolved (latestScheduledTime + latency) or later
                // only need to do +1 because +2 was already done when computing latestScheduledTime
                schedulerState.latestMult = (schedulerState.latestMult < latestScheduledTime + 1) ? latestScheduledTime + 1 : schedulerState.latestMult; // compute latestMult making sure the new entry is scheduled AT LEAST 3 cycles AFTER its dependency
                // create as many new VLIW bundles as the difference between the old latest and the new latest
                for (int j = oldLatest; j < schedulerState.latestMult; j++)
                {
                    VLIW v = newVLIW(state);
                    vliw = &v;
                }
                // schedule the instruction in the VLIW bundle with the latest scheduled time
                vliw->mult = &instrs.instructions[entry->ID - 65];
                entry->scheduledTime = schedulerState.latestMult;
                scheduleInstruction(state, entry, &schedulerState);
                schedulerState.latestMult = oldLatest; // restore old latest
            }
            else if (table->dependencies[IDdependsOn].type == LD || table->dependencies[IDdependsOn].type == ST)
            {
                int oldLatest = schedulerState.latestMem;
                schedulerState.latestMem = max(schedulerState.latestMem, latestScheduledTime + 1);
                scheduleInstruction(state, entry, &schedulerState);
                schedulerState.latestMem = oldLatest;
            }
            else
            {
                int oldLatest1 = schedulerState.latestALU1;
                int oldLatest2 = schedulerState.latestALU2;
                schedulerState.latestALU1 = max(schedulerState.latestALU1, latestScheduledTime + 1);
                schedulerState.latestALU2 = max(schedulerState.latestALU2, latestScheduledTime + 1);
                scheduleInstruction(state, entry, &schedulerState);
                schedulerState.latestALU1 = oldLatest1;
                schedulerState.latestALU2 = oldLatest2;
            }
        }

        //-->   // Interloop dependencies
        // handled right after all of BB1 is scheduled
        if (table->dependencies[instrs.loopEnd].scheduledTime != -1 && table->dependencies[instrs.loopEnd + 1].scheduledTime == -1)
        {
            for (int i = instrs.loopStart; i < instrs.loopEnd + 1; i++)
            {
                DependencyEntry *entry = &table->dependencies[i];

                if (entry->loop.size != 0)
                {
                    int latestScheduledTime = -1;
                    int IDdependsOn = -1;
                    // look for the latest scheduled time of the instruction(s) it depends on ONLY in the loop (i.e. block == 1)
                    for (int j = 0; j < entry->loop.size; j++)
                    {
                        if (table->dependencies[entry->loop.list[j].ID - 65].type == MULU)
                        {
                            if ((table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2) > latestScheduledTime)
                            {
                                latestScheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2;
                                IDdependsOn = entry->loop.list[j].ID - 65;
                            }
                        }
                        else if (table->dependencies[entry->loop.list[j].ID - 65].scheduledTime > latestScheduledTime)
                        {
                            latestScheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime;
                            IDdependsOn = entry->loop.list[j].ID - 65;
                        }
                    }

                    // only case that needs to be handled: depends on a MULU
                    if (table->dependencies[IDdependsOn].type == MULU)
                    {
                        // check that the distance between this instruction and the entry is at least 3 cycles

                        // distance between the beginning of the loop and the instruction scheduled time
                        int x = entry->scheduledTime - instrs.loopStart;
                        // distance between the end of the loop scheduled time and the instruction it depends on
                        int y = table->dependencies[instrs.loopEnd].scheduledTime - table->dependencies[IDdependsOn].scheduledTime;

                        // if the distance is less than 3, need to add NOP cycles
                        for (int j = 0; j < (3 - (x + y + 1)); j++)
                        {
                            VLIW v = newVLIW(state);
                            vliw = &v;
                        }
                        // postpone the LOOP or LOOP_PIP instruction to the end of the loop
                        vliw = &state->bundles.vliw[table->dependencies[instrs.loopEnd].scheduledTime];
                        vliw->br = &NOPinstr;

                        vliw = &state->bundles.vliw[state->bundles.size - 1];
                        vliw->br = &instrs.instructions[instrs.loopEnd];

                        schedulerState.latestBr = state->bundles.size - 1;
                        table->dependencies[instrs.loopEnd].scheduledTime = schedulerState.latestBr;
                    }
                }
            }
        }

        // Once the loop is scheduled, comute the number of loop stages
        state->stage = floor((table->dependencies[instrs.loopEnd].scheduledTime + 1 - table->dependencies[instrs.loopStart].scheduledTime) / state->II);

        // check if the II needs to be updated
        int changed = checkInterloopDependencies(table, state);

        if (changed)
        {
            // reschedule the instructions
            scheduleInstructionsPip(state, table);
        }
    }
}

/*************************************************************
 *
 *                    Parsing Methods
 *
 * ***********************************************************/

/**
 * @brief Add an instruction to the instruction set.
 */
void pushInstruction(InstructionEntry entry)
{
    instrs.instructions = realloc(instrs.instructions, (instrs.size + 1) * sizeof(InstructionEntry));

    instrs.instructions[instrs.size] = entry;
    instrs.size++;
}

/**
 * @brief Parse the input file and store the instructions in the instruction set.
 */
void parseInstrunctions(char *inputFile)
{

    FILE *file2 = fopen(inputFile, "r");

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

    fseek(file2, 0, SEEK_END);
    long file_size2 = ftell(file2);
    rewind(file2); // Go back to the beginning of the file

    // Allocate memory to store the file contents

    char *json_data2 = (char *)malloc(file_size2 + 1);
    if (json_data2 == NULL)
    {
        printf("Memory allocation failed.\n");
        fclose(file2);
        return;
    }
    // Read the file contents into the allocated memory

    fread(json_data2, 1, file_size2, file2);
    json_data2[file_size2] = '\0';

    // Close the file

    cJSON *root2 = cJSON_Parse(json_data2);
    // prog1 is an array of strings

    printf("root2\n");
    instrs.loopEnd = -1;
    instrs.loopStart = -1;
    for (int i = 0; i < cJSON_GetArraySize(root2); i++)
    {
        cJSON *instr = cJSON_GetArrayItem(root2, i);
        // convert instr to string
        char *instr_str = cJSON_Print(instr);
        // parse instr_str to InstructionEntry eg: "addi x1, x1, 1"
        InstructionEntry entry;

        parseString(instr_str, &entry);
        pushInstruction(entry);
        if (entry.type == LOOP || entry.type == LOOP_PIP)
        {
            instrs.loopStart = entry.imm;
            instrs.loopEnd = i;
        }

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
void showInstruction(InstructionEntry instr)
{
    printf("Instruction\n");
    printf("Opcode: %s\n", instr.opcode);
    printf("Block: %d\n", instr.block);
    printf("Dest: %d\n", instr.dest);
    printf("Src1: %d\n", instr.src1);
    printf("Src2: %d\n", instr.src2);
    printf("Imm: %d\n", instr.imm);
    printf("Pred: %d\n", instr.predicate);
    printf("Type: %d\n", instr.type);
}

/**
 * @brief Function to display the instruction set.
 */
void showInstructionSet()
{
    printf("Instructions\n");
    printf("Size: %d\n", instrs.size);
    printf("Loop Start: %d\n", instrs.loopStart);
    printf("Loop End: %d\n", instrs.loopEnd);

    for (int i = 0; i < instrs.size; i++)
    {
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
            printf("ID: %c, Reg: %d, \n", table.dependencies[i].local.list[j].ID, table.dependencies[i].local.list[j].reg);
        }
        printf("Loop\n");
        for (int j = 0; j < table.dependencies[i].loop.size; j++)
        {
            printf("ID: %c, Reg: %d src %d\n", table.dependencies[i].loop.list[j].ID, table.dependencies[i].loop.list[j].reg, table.dependencies[i].loop.list[j].src);
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
void showVLIW(VLIW vliw)
{
    printf("ALU1\n");
    showInstruction(*vliw.alu1);
    printf("ALU2\n");
    showInstruction(*vliw.alu2);
    printf("MULT\n");
    showInstruction(*vliw.mult);
    printf("MEM\n");
    showInstruction(*vliw.mem);
    printf("BR\n");
    showInstruction(*vliw.br);
}

/**
 * @brief Function to display the processor state.
 */
void showProcessorState(ProcessorState state)
{
    printf("Processor State\n");
    printf("PC: %d\n", state.PC);
    printf("LC: %d\n", state.LC);
    printf("EC: %d\n", state.EC);
    printf("RRB: %d\n", state.RRB);
    printf("II: %d\n", state.II);
    printf("PredRegFile\n");
    for (int i = 0; i < REGS; i++)
    {
        printf("%d: %d\n", i, state.PredRegFile[i]);
    }
    printf("PhysRegFile\n");
    for (int i = 0; i < REGS; i++)
    {
        printf("%d: %d\n", i, state.PhysRegFile[i]);
    }
    printf("Bundles\n");
    for (int i = 0; i < state.bundles.size; i++)
    {
        printf("VLIW %d\n", i);
        showVLIW(state.bundles.vliw[i]);
    }
}