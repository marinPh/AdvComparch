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
    state->II = calculateIIRes(state);
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
void pushId(idList *list, char id, unsigned int reg, int src)
{
    list->list = realloc(list->list, (list->size + 1) * sizeof(dependency));
    list->list[list->size].ID = id;
    list->list[list->size].reg = reg;
    list->list[list->size].src = src;
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
    //  we check if the dest of instr1 is equal to src1 or src2 of instr2

    if (instrs.instructions[instr1].dest == instrs.instructions[instr2].src1 || instrs.instructions[instr1].dest == instrs.instructions[instr2].src2)
    {
        // src is the name of the src that has the dependency
        int src = instrs.instructions[instr1].dest == instrs.instructions[instr2].src1 ? 1 : 2;
        int srcOther = -1;
        if (instrs.instructions[instr1].dest == instrs.instructions[instr2].src1 && instrs.instructions[instr1].dest == instrs.instructions[instr2].src2) 
        {
            srcOther = 2; 
        }

        if (instrs.loopStart != -1)
        { // check if in same block

            if (instrs.instructions[instr1].block == instrs.instructions[instr2].block)
            {
                // check if instr1 > instr2

                if (instr1 < instr2)
                {
                    pushId(&entry->local, table->dependencies[instr1].ID, instrs.instructions[instr1].dest, src);
                    if (srcOther != -1)
                    {
                        pushId(&entry->local, table->dependencies[instr1].ID, instrs.instructions[instr1].dest, srcOther);
                    }
                }
                else
                {
                    pushId(&entry->loop, table->dependencies[instr1].ID, instrs.instructions[instr1].dest, src);
                    if (srcOther != -1)
                    {
                        pushId(&entry->loop, table->dependencies[instr1].ID, instrs.instructions[instr1].dest, srcOther);
                    }
                }
            }
            else
            {
                // check if in post loop means instr1 is in block 1 and instr2 is in block 2
                if (instrs.instructions[instr1].block == 1 && instrs.instructions[instr2].block == 2)
                {
                    pushId(&entry->postL, table->dependencies[instr1].ID, instrs.instructions[instr1].dest, src);
                    if (srcOther != -1)
                    {
                        pushId(&entry->postL, table->dependencies[instr1].ID, instrs.instructions[instr1].dest, srcOther);
                    }
                }
                else
                {
                    // check if in invariant
                    if (instrs.instructions[instr1].block == 0 && (instrs.instructions[instr2].block == 1 || instrs.instructions[instr2].block == 2))
                    {
                        pushId(&entry->invariant, table->dependencies[instr1].ID, instrs.instructions[instr1].dest, src);
                        if (srcOther != -1)
                        {
                            pushId(&entry->invariant, table->dependencies[instr1].ID, instrs.instructions[instr1].dest, srcOther);
                        }
                    }
                }
            }
        }
        else
        {
            // if there is no loop all dependencies are local
            pushId(&entry->local, table->dependencies[instr1].ID, instrs.instructions[instr1].dest, src);
            if (srcOther != -1)
            {
                pushId(&entry->local, table->dependencies[instr1].ID, instrs.instructions[instr1].dest, srcOther);
            }
        }
    }
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
        int pot = instrs.instructions[i].dest;

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

        if (instrs.instructions[i].block == 1 && instrs.loopStart != -1)
        {
            for (int j = instrs.loopStart; j <= i; j++)
            {
                whatType(i, j, &table);
            }
        }
    }
    printf("Size of loop for entry 2: %d\n", table.dependencies[2].loop.size);

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
    //TODO: check unexpected behavior
    II_res = (counts[ADD] + counts[ADDI] + counts[SUB] + counts[MOV] + state->FUCount[0] - 1) / state->FUCount[0];
    II_res = fmax(II_res, (counts[MULU] + state->FUCount[1] - 1) / state->FUCount[1]);
    II_res = fmax(II_res, (counts[LD] + state->FUCount[2] - 1) / state->FUCount[2]);
    II_res = fmax(II_res, (counts[LOOP] + counts[LOOP_PIP] + state->FUCount[3] - 1) / state->FUCount[3]);
    printf("II_res: %d\n", II_res);
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


    for (int i = 0; i < current->loop.size; i++)
    {
        //TODO: wrong typ casting
        dependency *dependency = &current->loop.list[i];
        printf("%d\n",i);
        // Check the interloop dependency condition

        printf("dep: %d, %d\n",dependency->ID,current->scheduledTime);
        if (current->scheduledTime + 1 > current->scheduledTime + state->II)
        {
            // Adjust the II to satisfy the interloop dependency
            state->II = current->scheduledTime + 1 - current->scheduledTime;
            printf("II adjusted to %d\n", state->II);
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
    int latency = instrs.instructions[instrs.loopEnd].type == LOOP ? 2 : 0;

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
            if (table->dependencies[entry->local.list[j].ID - 65].type == MULU)
            {
                if ((table->dependencies[entry->local.list[j].ID - 65].scheduledTime + latency) > latestScheduledTime)
                {
                    latestScheduledTime = table->dependencies[entry->local.list[j].ID - 65].scheduledTime + latency;
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
            if (table->dependencies[entry->local.list[j].ID - 65].type == MULU)
            {
                if ((table->dependencies[entry->local.list[j].ID - 65].scheduledTime + latency) > latestScheduledTimeOtherSrc)
                {
                    latestScheduledTimeOtherSrc = table->dependencies[entry->local.list[j].ID - 65].scheduledTime + latency;
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

    // invariantDeps
    for (int j = 0; j < entry->invariant.size; j++)
    {
        if (entry->invariant.list[j].reg == reg1)
        {
            if (table->dependencies[entry->invariant.list[j].ID - 65].type == MULU)
            {
                if ((table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime + latency) > latestScheduledTime)
                {
                    latestScheduledTime = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime + latency;
                    IDdependsOn = entry->invariant.list[j].ID - 65;
                }
            }
            else if (table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime > latestScheduledTime)
            {
                latestScheduledTime = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime;
                IDdependsOn = entry->invariant.list[j].ID - 65;
            }
            latestDep.depType = INVARIANT;
        }
        if (entry->invariant.list[j].reg == reg2)
        {
            if (table->dependencies[entry->invariant.list[j].ID - 65].type == MULU)
            {
                if ((table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime + latency) > latestScheduledTimeOtherSrc)
                {
                    latestScheduledTimeOtherSrc = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime + latency;
                    IDdependsOnOtherSrc = entry->invariant.list[j].ID - 65;
                }
            }
            else if (table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime > latestScheduledTimeOtherSrc)
            {
                latestScheduledTimeOtherSrc = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime;
                IDdependsOnOtherSrc = entry->invariant.list[j].ID - 65;
            }
            latestDep.depTypeOtherSrc = INVARIANT;
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
                if ((table->dependencies[entry->postL.list[j].ID - 65].scheduledTime + latency) > latestScheduledTime)
                {
                    latestScheduledTime = table->dependencies[entry->postL.list[j].ID - 65].scheduledTime + latency;
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
                if ((table->dependencies[entry->postL.list[j].ID - 65].scheduledTime + latency) > latestScheduledTimeOtherSrc)
                {
                    latestScheduledTimeOtherSrc = table->dependencies[entry->postL.list[j].ID - 65].scheduledTime + latency;
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

    latestDep.idxOtherSrc = IDdependsOnOtherSrc;
    latestDep.destOtherSrc = table->dependencies[IDdependsOnOtherSrc].dest;
    latestDep.scheduledTimeOtherSrc = latestScheduledTimeOtherSrc;
    latestDep.blockOtherSrc = instrs.instructions[IDdependsOnOtherSrc].block;

    return latestDep;
}

InstructionEntry *createNewInstruction(
    char *opcode,
    int block,
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
    instr->block = block;
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

int allocateUnused(ProcessorState *state, DependencyTable *table, int reg) 
{
        // reorder instructions IDs in a temporary array in ascending scheduled time order
    int instrsIDs[instrs.size];
    for (int i = 0; i < instrs.size; i++)
    {
        instrsIDs[i] = i;
    }
    for (int i = 0; i < instrs.size; i++)
    {
        for (int j = i + 1; j < instrs.size; j++)
        {
            if (table->dependencies[instrsIDs[i]].scheduledTime > table->dependencies[instrsIDs[j]].scheduledTime)
            {
                int temp = instrsIDs[i];
                instrsIDs[i] = instrsIDs[j];
                instrsIDs[j] = temp;
            }
            if (table->dependencies[instrsIDs[i]].scheduledTime == table->dependencies[instrsIDs[j]].scheduledTime)
            {
                if (instrs.instructions[instrsIDs[i]].type > instrs.instructions[instrsIDs[j]].type)
                {
                    // if both are in ALUs, the one in ALU1 is renamed first
                    if (state->bundles.vliw[table->dependencies[instrsIDs[i]].scheduledTime].alu1->type == instrs.instructions[instrsIDs[i]].type
                        && state->bundles.vliw[table->dependencies[instrsIDs[i]].scheduledTime].alu1->dest == instrs.instructions[instrsIDs[i]].dest) 
                    {
                        continue;
                    }
                    int temp = instrsIDs[i];
                    instrsIDs[i] = instrsIDs[j];
                    instrsIDs[j] = temp;
                }
            }
        }
    }
    
    for (int j = 0; j < instrs.size; j++)
    {
        int k = instrsIDs[j];
        DependencyEntry *entry = &table->dependencies[k];

        // check if src1 has a dependency
        bool hasDep1 = false;
        for (int l = 0; l < entry->local.size; l++)
        {
            if (entry->local.list[l].src == 1)
            {
                hasDep1 = true;
                break;
            } 
        }
        if (!hasDep1) {
            for (int l = 0; l < entry->loop.size; l++)
            {
                if (entry->loop.list[l].src == 1)
                {
                    hasDep1 = true;
                    break;
                }
            }
        }
        if (!hasDep1) {
            for (int l = 0; l < entry->invariant.size; l++)
            {
                if (entry->invariant.list[l].src == 1)
                {
                    hasDep1 = true;
                    break;
                }
            }
        }
        if (!hasDep1) {
            for (int l = 0; l < entry->postL.size; l++)
            {
                if (entry->postL.list[l].src == 1)
                {
                    hasDep1 = true;
                    break;
                }
            }
        }

        // check if src2 has a dependency
        bool hasDep2 = false;
        for (int l = 0; l < entry->local.size; l++)
        {
            if (entry->local.list[l].src == 2)
            {
                hasDep2 = true;
                break;
            }
        }
        if (!hasDep2) {
            for (int l = 0; l < entry->loop.size; l++)
            {
                if (entry->loop.list[l].src == 2)
                {
                    hasDep2 = true;
                    break;
                }
            }
        }
        if (!hasDep2) {
            for (int l = 0; l < entry->invariant.size; l++)
            {
                if (entry->invariant.list[l].src == 2)
                {
                    hasDep2 = true;
                    break;
                }
            }
        }
        if (!hasDep2) {
            for (int l = 0; l < entry->postL.size; l++)
            {
                if (entry->postL.list[l].src == 2)
                {
                    hasDep2 = true;
                    break;
                }
            }
        }

        // special case for ST because saved in opposite order 
        if (instrs.instructions[k].type == ST)
        {
            if (!hasDep2)
            {
                instrs.instructions[k].src2 = reg;
                reg++;
            }

            if (!hasDep1)
            {
                instrs.instructions[k].src1 = reg;
                reg++;
            }
        }
        else
        {
            // allocate a register to the source register
            if (!hasDep1 && instrs.instructions[k].src1 != -1)
            {
                instrs.instructions[k].src1 = reg;
                reg++;
            }

            if (!hasDep2 && instrs.instructions[k].src2 != -1)
            {
                instrs.instructions[k].src2 = reg;
                reg++;
            }
        }
    }

    return reg;
}


int renameDestReg(InstructionEntry *FuncUnit, ProcessorState *state, DependencyTable *table, int reg) 
{
    if (FuncUnit->type != NOP && FuncUnit->dest > -3 && FuncUnit->predicate == -1)
    {
        // Allocate registers
        bool dependance = false;

        if (instrs.loopStart != -1)
        {
            for (int j = instrs.loopStart; j < instrs.loopEnd + 1; j++)
            {
                DependencyEntry *entry = &table->dependencies[j];
                for (int k = 0; k < entry->loop.size; k++)
                {
                    if (entry->loop.list[k].reg == FuncUnit->dest)
                    {
                        int stageDiff = floor((table->dependencies[entry->loop.list[k].ID - 65].scheduledTime - table->dependencies[instrs.loopStart].scheduledTime) / state->II);
                        FuncUnit->dest = instrs.instructions[entry->loop.list[k].ID - 65].dest; // -S(P), iteration = 1    TODO  - stageDiff + 1
                        dependance = true;
                    }
                }
            }
            if (!dependance)
            {
                FuncUnit->dest = reg;
                reg++;
            }
        }
    }

    return reg; 
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

        // interloopDeps
        // only check for interloop dependencies if the latest found dependency producer is NOT in BB0
        // (case where there exist 2 producers, one in BB0 and one in BB1, and the consumer is in BB1 or BB2)    TODO consumer in BB2 ?
        if (!(latestDep.block == 0 && latestDep.dest == reg1))
        { // check interloop for reg1            
            for (int j = 0; j < entry->loop.size; j++)
            {
                // skip the case where both source registers are the same
                if (entry->loop.list[j].ID == entry->ID)
                {
                    continue;
                }

                if (entry->loop.list[j].reg == reg1)
                {

                    if (table->dependencies[entry->loop.list[j].ID - 65].type == MULU)
                    {
                        if ((table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2) > latestDep.scheduledTime)
                        {
                            latestDep.scheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2;
                            latestDep.idx = entry->loop.list[j].ID - 65;
                        }
                    }
                    else if (table->dependencies[entry->loop.list[j].ID - 65].scheduledTime > latestDep.scheduledTime)
                    {
                        latestDep.scheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime;
                        latestDep.idx = entry->loop.list[j].ID - 65;
                    }
                }
            }
        }

        if (!(latestDep.blockOtherSrc == 0 && latestDep.destOtherSrc == reg2))
        { // check interloop for reg2

            for (int j = 0; j < entry->loop.size; j++)
            {
                // skip the case where both source registers are the same
                if (entry->loop.list[j].ID == entry->ID)
                {
                    continue;
                }

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
            }
            else if (table->dependencies[latestDep.idx].dest == reg2)
            {
                instrs.instructions[entry->ID - 65].src2 = instrs.instructions[latestDep.idx].dest;
            }
        }
        if (latestDep.idxOtherSrc != -1)
        {
            if (table->dependencies[latestDep.idxOtherSrc].dest == reg2)
            {
                instrs.instructions[entry->ID - 65].src2 = instrs.instructions[latestDep.idxOtherSrc].dest;
            }
            else if (table->dependencies[latestDep.idxOtherSrc].dest == reg1)
            {
                instrs.instructions[entry->ID - 65].src1 = instrs.instructions[latestDep.idxOtherSrc].dest;
            }
        }
    }

    // Step 3. fix interloopDeps by scheduling MOV instructions
    // for every instruction in DependencyTable, check if it has an interloop dependency with itself as a producer
    // if yes, schedule a MOV instruction to copy the value of the source register to the destination register
    for (int j = 0; j < table->size; j++)
    {
        DependencyEntry *entry = &table->dependencies[j];
        // check if it has a dependency with itself as a producer

        bool doubleDep = false;

        for (int k = 0; k < entry->loop.size; k++)
        {
            if (entry->loop.list[k].ID == entry->ID)
            {
                // skip the case where both source registers are the same 
                // ex.  ADD x1, x1, x1  
                // only add 1 MOV instruction (not 2)
                if (doubleDep)
                {
                    continue;
                }

                int reg = entry->loop.list[k].src == 1 ? instrs.instructions[entry->ID - 65].src1 : instrs.instructions[entry->ID - 65].src2;

                // if the register was used in the loop as a destination register BEFORE the current instruction, continue
                // i.e. another instruction writes to it before the current instruction reads from it
                bool usedBefore = false;
                for (int l = instrs.loopStart; l < j; l++)
                {
                    if (instrs.instructions[l].dest == reg && table->dependencies[l].scheduledTime < entry->scheduledTime)
                    {
                        usedBefore = true;
                        break;
                    }
                }
                if (usedBefore)
                {
                    continue;
                }

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
                        }
                    }
                }

                if (type == MULU)
                {
                    lastUsedTime += 3;
                } 
                else if (instrs.loopStart == (instrs.loopEnd - 1) || 
                         entry->scheduledTime == lastUsedTime) {  
                         // if the instruction is the last one in the loop
                         // or it was scheduled at the same cycle as the last instruction that used the register it depends on
                    lastUsedTime += 1;
                }

                // schedule a MOV instruction to copy the value of the source register to the destination register
                // in the VLIW bundle with the latest scheduled instruction if one of the two ALUs is free
                VLIW *vliw = &(state->bundles.vliw[lastUsedTime]);


                if (vliw->alu1->type == NOP)
                {
                    vliw->alu1 = createNewInstruction("mov", 1, regDep, instrs.instructions[entry->ID - 65].dest, -1, -1, -1, MOV, 0, false);
                }
                else if (vliw->alu2->type == NOP)
                {
                    vliw->alu2 = createNewInstruction("mov", 1, regDep, instrs.instructions[entry->ID - 65].dest, -1, -1, -1, MOV, 0, false);
                }

                // skip the case where both source registers are the same 
                // ex.  ADD x1, x1, x1  
                // only add 1 MOV instruction (not 2)
                if (entry->loop.list[k].reg == instrs.instructions[entry->ID - 65].src2 && instrs.instructions[entry->ID - 65].src2 == instrs.instructions[entry->ID - 65].src1) {
                    doubleDep = true;
                }
            }
        }
    }

    // Step 4. allocate unused register for all source registers that have no RAW dependency
    // for every instruction in DependencyTable, check if it has a dependency on another instruction
    // if not, allocate a register to the source register

    reg = allocateUnused(state, table, reg);

    if(instrs.loopEnd != -1 ){
        state->bundles.vliw[table->dependencies[instrs.loopEnd].scheduledTime].br->imm = table->dependencies[instrs.loopStart].scheduledTime;
    }
}

/**
 * @brief Rename the scheduled instructions to eliminate WARs and WAWs when LOOP_PIP.
 * @param state The pointer to the processor state.
 * @param table The pointer to the dependency table.
 */
void registerAllocationPip(ProcessorState *state, DependencyTable *table)
{
    // Allocate registers to instructions in the VLIW bundle based on the DependencyTable

    int offset = 0; // offset for rotating registers
    int reg = 1;    // for non-rotating registers

    // Step 0. destination register renaming and local dependency renaming for BB0 and BB2
    // rename the destination registers in BB0
    for (int i = 0; i < table->dependencies[instrs.loopStart].scheduledTime; i++)
    {
        VLIW *vliw = &state->bundles.vliw[i];

        // ALU1
        reg = renameDestReg(vliw->alu1, state, table, reg);

        // ALU2
        reg = renameDestReg(vliw->alu2, state, table, reg);

        // MULT
        reg = renameDestReg(vliw->mult, state, table, reg);

        // MEM
        reg = renameDestReg(vliw->mem, state, table, reg);
    }

    printf("Step 0 done\n");

    // BB2
    for (int i = table->dependencies[instrs.loopEnd].scheduledTime + 1; i < state->bundles.size; i++)
    {
        VLIW *vliw = &state->bundles.vliw[i];
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

    printf("BB2 renaming done\n");

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

    printf("Local deps in BB0 done \n");

    // TODO separate state in 1 VLIW bundles for BB0, 1 for BB1 and 1 for BB2 ?

    printf("start loop schedule time: %d\n", table->dependencies[instrs.loopStart].scheduledTime);
    printf("end loop schedule time: %d\n", table->dependencies[instrs.loopEnd].scheduledTime);

    printf("RRB: %d\n", state->RRB);
    printf("stage: %d\n", state->stage);

    // Step 1. allocate ROTATING registers to the destination registers of all instructions in the VLIW bundles
    for (int i = table->dependencies[instrs.loopStart].scheduledTime; i < table->dependencies[instrs.loopEnd].scheduledTime + 1; i++)
    {
        VLIW *vliw = &state->bundles.vliw[i];
        // ALU1
        if (vliw->alu1->type != NOP && vliw->alu1->dest > 0)
        {
            // Allocate registers
            vliw->alu1->dest = offset + ROTATION_START_INDEX + state->RRB;
            offset++;
            offset += state->stage;
            printf("ALU1 Offset: %d\n", offset);
        }
        // ALU2
        if (vliw->alu2->type != NOP && vliw->alu2->dest > 0)
        {
            vliw->alu2->dest = offset + ROTATION_START_INDEX + state->RRB;
            offset++;
            offset += state->stage;
            printf("ALU2 Offset: %d\n", offset);
        }
        // MULT
        if (vliw->mult->type != NOP && vliw->mult->dest > 0)
        {
            vliw->mult->dest = offset + ROTATION_START_INDEX + state->RRB;
            offset++;
            offset += state->stage;
            printf("MULT Offset: %d\n", offset);
        }
        // MEM
        printf("RRB: %d\n", state->RRB);
        if (vliw->mem->type == LD && vliw->mem->dest > 0)
        { // only rename LDs (STs are not renamed)
            vliw->mem->dest = offset + ROTATION_START_INDEX + state->RRB;
            offset++;
            offset += state->stage;
            printf("MEM Offset: %d\n", offset);
        }
    }

    printf("dest rotational regs done\n");

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

    printf("local deps in BB2\n");

    // Step 3. for local and interloop dependencies, adjust the src registers by adding the difference
    // of the stage of the consumer and the stage of the producer (+1 for interloop)
    // (a new stage every II cycles, a new loop => new stage too)

    for (int k = instrs.loopStart; k < instrs.loopEnd + 1; k++)
    {
        DependencyEntry *entry = &table->dependencies[k];

        // find the latest dependency for src1 and src2
        int reg1 = instrs.instructions[entry->ID - 65].src1; // source register 1
        int reg2 = instrs.instructions[entry->ID - 65].src2; // source register 2

        int latestDep1 = -1;
        int latestDep2 = -1;

        int stageDiff1 = floor((table->dependencies[entry->ID - 65].scheduledTime - table->dependencies[instrs.loopStart].scheduledTime) / state->II);
        int stageDiff2 = floor((table->dependencies[entry->ID - 65].scheduledTime - table->dependencies[instrs.loopStart].scheduledTime) / state->II);

        printf("II: %d\n", state->II);
        printf("stageDiff1: %d\n", stageDiff1);
        printf("stageDiff2: %d\n", stageDiff2);
        // set the cycle variable to the stage of the instruction + ROTATION_START_INDEX
        instrs.instructions[entry->ID - 65].cycle = stageDiff1 + ROTATION_START_INDEX;

        for (int j = 0; j < entry->local.size; j++)
        {
            // MUST use instrs.instructions[entry->local.list[j].ID-65].dest
            // bc table->dependencies[entry->local.list[j].ID-65].dest NOT renamed
            if (entry->local.list[j].reg == reg1 && table->dependencies[entry->local.list[j].ID - 65].scheduledTime > latestDep1)
            {
                int stageDiff = floor((table->dependencies[entry->local.list[j].ID - 65].scheduledTime - table->dependencies[instrs.loopStart].scheduledTime) / state->II);

                instrs.instructions[entry->ID - 65].src1 = instrs.instructions[entry->local.list[j].ID - 65].dest + (stageDiff1 - stageDiff);
                latestDep1 = table->dependencies[entry->local.list[j].ID - 65].scheduledTime;
            }
            if (entry->local.list[j].reg == reg2 && table->dependencies[entry->local.list[j].ID - 65].scheduledTime > latestDep2)
            {
                int stageDiff = floor((table->dependencies[entry->local.list[j].ID - 65].scheduledTime - table->dependencies[instrs.loopStart].scheduledTime) / state->II);

                instrs.instructions[entry->ID - 65].src2 = instrs.instructions[entry->local.list[j].ID - 65].dest + (stageDiff2 - stageDiff);
                latestDep2 = table->dependencies[entry->local.list[j].ID - 65].scheduledTime;
            }
        }

        // reset
        // latestDep1 = -1;
        // latestDep2 = -1;

        for (int j = 0; j < entry->loop.size; j++)
        {
            if (entry->loop.list[j].reg == reg1 && table->dependencies[entry->loop.list[j].ID - 65].scheduledTime > latestDep1)
            {
                int stageDiff = floor((table->dependencies[entry->loop.list[j].ID - 65].scheduledTime - table->dependencies[instrs.loopStart].scheduledTime) / state->II);
                printf("stageDiff  reg1: %d\n", stageDiff);
                instrs.instructions[entry->ID - 65].src1 = instrs.instructions[entry->loop.list[j].ID - 65].dest + (stageDiff1 - stageDiff) + 1;
                printf("src1: %d\n", instrs.instructions[entry->ID - 65].src1);

                latestDep1 = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime;
            }
            if (entry->loop.list[j].reg == reg2 && table->dependencies[entry->loop.list[j].ID - 65].scheduledTime > latestDep2)
            {
                int stageDiff = floor((table->dependencies[entry->loop.list[j].ID - 65].scheduledTime - table->dependencies[instrs.loopStart].scheduledTime) / state->II);
                instrs.instructions[entry->ID - 65].src2 = instrs.instructions[entry->loop.list[j].ID - 65].dest + (stageDiff2 - stageDiff) + 1;
                latestDep2 = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime;
            }
        }

        // TODO what if both local and interloop ?

        // Step 2. allocate non-rotating register to the invariant dependencies
        // but only if NOT ALSO a local or interloop dependency
        for (int j = 0; j < entry->invariant.size; j++)
        {
            // only rename if it is not ALSO an interloop dependency or a local dependency
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

    // // Step 2. allocate non-rotating register to the invariant dependencies
    // for (int j = instrs.loopStart; j < instrs.loopEnd + 1; j++)
    // {
    //     DependencyEntry *entry = &table->dependencies[j];
    //     if (entry->invariant.size != 0)
    //     {
    //         // find the latest dependency for src1 and src2
    //         int reg1 = instrs.instructions[entry->ID - 65].src1; // source register 1
    //         int reg2 = instrs.instructions[entry->ID - 65].src2; // source register 2

    //         int latestDep1 = -1;
    //         int latestDep2 = -1;

    //         for (int k = 0; k < entry->invariant.size; k++)
    //         {
    //             // only rename if it is not ALSO an interloop dependency or a local dependency
    //             if (entry->invariant.list[j].reg == reg1 && table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime > latestDep1)
    //             {
    //                 instrs.instructions[entry->ID - 65].src1 = table->dependencies[entry->invariant.list[j].ID - 65].dest;
    //                 latestDep1 = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime;
    //             }
    //             if (entry->invariant.list[j].reg == reg2 && table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime > latestDep2)
    //             {
    //                 instrs.instructions[entry->ID - 65].src2 = table->dependencies[entry->invariant.list[j].ID - 65].dest;
    //                 latestDep2 = table->dependencies[entry->invariant.list[j].ID - 65].scheduledTime;
    //             }
    //         }
    //     }
    // }

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
    reg = allocateUnused(state, table, reg);

    if(instrs.loopEnd != -1 ){
        state->bundles.vliw[table->dependencies[instrs.loopEnd].scheduledTime].br->imm  = table->dependencies[instrs.loopStart].scheduledTime;
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
        while (schedulerState->latestALU1 < state->bundles.size || schedulerState->latestALU2 < state->bundles.size)
        { // latestALU1 == latestALU2 for dependencies
            if (schedulerState->latestALU1 < state->bundles.size && schedulerState->latestALU1 <= schedulerState->latestALU2)
            { // ALU1 only chosen if it's not later than a free ALU2
                vliw = &state->bundles.vliw[schedulerState->latestALU1];

                if (vliw->alu1->type == NOP)
                {
                    vliw = &state->bundles.vliw[schedulerState->latestALU1]; // take the vliw bundle at the latestALU1 index
                    vliw->alu1 = &instrs.instructions[entry->ID - 65];
                    entry->scheduledTime = schedulerState->latestALU1;
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
                vliw = &state->bundles.vliw[schedulerState->latestALU2];
                if (vliw->alu2->type == NOP)
                {
                    vliw = &state->bundles.vliw[schedulerState->latestALU2];
                    vliw->alu2 = &instrs.instructions[entry->ID - 65];
                    entry->scheduledTime = schedulerState->latestALU2;
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
            newVLIW(state);
            vliw = &(state->bundles.vliw[state->bundles.size - 1]);
            vliw->alu1 = &instrs.instructions[entry->ID - 65];
            entry->scheduledTime = schedulerState->latestALU1; // latestALU1 will be == the size of the bundles-1
            schedulerState->latestALU1 += 1;
        }
    }
    else if (entry->type == MULU)
    {
        while (schedulerState->latestMult < state->bundles.size)
        {
            vliw = &state->bundles.vliw[schedulerState->latestMult];
            if (vliw->mult->type == NOP)
            {
                vliw = &state->bundles.vliw[schedulerState->latestMult];
                vliw->mult = &instrs.instructions[entry->ID - 65];
                entry->scheduledTime = schedulerState->latestMult;
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
        printf("latestMem before: %d\n", schedulerState->latestMem);
        while (schedulerState->latestMem < state->bundles.size)
        {
            vliw = &state->bundles.vliw[schedulerState->latestMem];
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
            printf("scheduled mem: %d\n", schedulerState->latestMem);
            newVLIW(state);
            vliw = &state->bundles.vliw[state->bundles.size - 1];
            vliw->mem = &instrs.instructions[entry->ID - 65];
            entry->scheduledTime = schedulerState->latestMem;
            schedulerState->latestMem += 1;
        }
    }
    printf("Entry ID: %d\n", entry->ID);
    printf("latestBr: %d\n", schedulerState->latestBr);
    printf("scheduledTime: %d\n", entry->scheduledTime);

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
            vliw = &state->bundles.vliw[schedulerState->latestBr];
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

    SchedulerState schedulerState;
    schedulerState.latestALU1 = 0;
    schedulerState.latestALU2 = 0;
    schedulerState.latestMult = 0;
    schedulerState.latestMem  = 0;
    schedulerState.latestBr   = 0;

    // create a new VLIW bundle
    newVLIW(state);
    VLIW *vliw = &state->bundles.vliw[state->bundles.size - 1];

    for (int i = 0; i < table->size; i++)
    {
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
            schedulerState.latestMem  = state->bundles.size - 1;
            schedulerState.latestBr   = state->bundles.size - 1;
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

                if (entry->type == ADD || entry->type == ADDI || entry->type == SUB || entry->type == MOV)
                {
                    int oldLatest = schedulerState.latestALU1;
                    int oldLatest2 = schedulerState.latestALU2;
                    schedulerState.latestALU1 = max(oldLatest, latestScheduledTime + 1);
                    schedulerState.latestALU2 = max(oldLatest2, latestScheduledTime + 1);

                    int minALUTime = (schedulerState.latestALU1 < schedulerState.latestALU2) ? schedulerState.latestALU1 : schedulerState.latestALU2;

                    for (int j = oldLatest; j < minALUTime-1; j++)
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
                    for (int j = oldLatest; j < schedulerState.latestMem-1; j++)
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
                    for (int j = oldLatest; j < schedulerState.latestMult-1; j++)
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
            // also handle the case where the loop is the last instruction
            if ((instrs.loopEnd == (instrs.size - 1) && table->dependencies[instrs.loopEnd].scheduledTime != -1) || table->dependencies[instrs.loopEnd].scheduledTime != -1 && table->dependencies[instrs.loopEnd + 1].scheduledTime == -1)
            {
                for (int i = instrs.loopStart; i < instrs.loopEnd; i++)
                {
                    DependencyEntry *entry = &table->dependencies[i];

                    if (entry->loop.size != 0)
                    {
                        int latestScheduledTime = -1;
                        int IDdependsOn = -1;

                        //bool doubleDep = false; // flag for cases like  ex. add x3 x3 x3

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

                        // first case that needs to be handled: depends on a MULU
                        if (table->dependencies[IDdependsOn].type == MULU)
                        {
                            // check that the distance between this instruction and the entry is at least 3 cycles

                            // distance between the beginning of the loop and the instruction scheduled time
                            int x = entry->scheduledTime - table->dependencies[instrs.loopStart].scheduledTime;
                            // distance between the end of the loop scheduled time and the instruction it depends on
                            int y = table->dependencies[instrs.loopEnd].scheduledTime - table->dependencies[IDdependsOn].scheduledTime;

                            // if the distance is less than 3, need to add NOP cycles
                            for (int j = 0; j < (3 - (x + y + 1)); j++)
                            {
                                newVLIW(state);
                                VLIW *vliw = &state->bundles.vliw[state->bundles.size - 1];
                            }

                            // postpone the LOOP or LOOP_PIP instruction to the end of the loop
                            vliw = &state->bundles.vliw[table->dependencies[instrs.loopEnd].scheduledTime];
                            vliw->br = &NOPinstr;

                            vliw = &state->bundles.vliw[state->bundles.size - 1];
                            vliw->br = &instrs.instructions[instrs.loopEnd];

                            schedulerState.latestBr = state->bundles.size - 1;
                            table->dependencies[instrs.loopEnd].scheduledTime = schedulerState.latestBr;
                        }

                        // second case to handle: the loop size is 1 
                        else if (table->dependencies[instrs.loopStart].scheduledTime == table->dependencies[instrs.loopEnd].scheduledTime) 
                        {
                            newVLIW(state);
                            VLIW *vliw = &state->bundles.vliw[state->bundles.size - 1];

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

void rescheduleLoop(ProcessorState *state, DependencyTable *table, SchedulerState *schedulerState) 
{
    VLIW *vliw = NULL;

    for (int i = instrs.loopStart; i < instrs.loopEnd+1; i++)
    {
        DependencyEntry *entry = &table->dependencies[i];

        // If there are no local, invariant or post loop dependencies (CAN have interloop dependencies !)
        if (entry->local.size == 0 && entry->invariant.size == 0 && entry->postL.size == 0)
        {
            // Schedule the instruction in the VLIW bundle
            // updated list of latest used ALU1, ALU2, MULT, MEM, BR, and loop instruction indices
            scheduleInstruction(state, entry, schedulerState);
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
                if (entry->type == ADD || entry->type == ADDI || entry->type == SUB || entry->type == MOV)
                {
                    int oldLatest  = schedulerState->latestALU1;
                    int oldLatest2 = schedulerState->latestALU2;
                    schedulerState->latestALU1 = max(oldLatest, latestScheduledTime + 1);
                    schedulerState->latestALU2 = max(oldLatest2, latestScheduledTime + 1);

                    int minALUTime = (schedulerState->latestALU1 < schedulerState->latestALU2) ? schedulerState->latestALU1 : schedulerState->latestALU2;

                    // for (int j = oldLatest; j < minALUTime-1; j++)
                    // {
                    //     newVLIW(state);
                    //     vliw = &state->bundles.vliw[state->bundles.size - 1];
                    // }
                    // schedule the instruction in the VLIW bundle with the latest scheduled time
                    // vliw->mult = &instrs.instructions[entry->ID-65];
                    // entry->scheduledTime = schedulerState->latestMult;

                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState->latestALU1 = oldLatest;
                    schedulerState->latestALU2 = oldLatest2;
                }
                else if (entry->type == LD || entry->type == ST)
                {
                    int oldLatest = schedulerState->latestMem;
                    schedulerState->latestMem = max(oldLatest, latestScheduledTime + 1);
                    // for (int j = oldLatest; j < schedulerState->latestMem-1; j++)
                    // {
                    //     newVLIW(state);
                    //     vliw = &state->bundles.vliw[state->bundles.size - 1];
                    // }
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState->latestMem = oldLatest;
                }
                else if (entry->type == MULU)
                {
                    int oldLatest = schedulerState->latestMult;
                    schedulerState->latestMult = max(oldLatest, latestScheduledTime + 1);
                    // for (int j = oldLatest; j < schedulerState->latestMult-1; j++)
                    // {
                    //     newVLIW(state);
                    //     vliw = &state->bundles.vliw[state->bundles.size - 1];
                    // }
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState->latestMult = oldLatest;
                }

                // new entry must be scheduled AFTER its dependency is resolved (latestScheduledTime + latency) or later
                // only need to do +1 because +2 was already done when computing latestScheduledTime
                // create as many new VLIW bundles as the difference between the old latest and the new latest
            }
            else if (table->dependencies[IDdependsOn].type == LD || table->dependencies[IDdependsOn].type == ST)
            {
                if (entry->type == ADD || entry->type == ADDI || entry->type == SUB || entry->type == MOV)
                {
                    int oldLatest = schedulerState->latestALU1;
                    int oldLatest2 = schedulerState->latestALU2;
                    schedulerState->latestALU1 = max(oldLatest, latestScheduledTime + 1);
                    schedulerState->latestALU2 = max(oldLatest2, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState->latestALU1 = oldLatest;
                    schedulerState->latestALU2 = oldLatest2;
                }
                else if (entry->type == LD || entry->type == ST)
                {
                    int oldLatest = schedulerState->latestMem;
                    schedulerState->latestMem = max(oldLatest, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState->latestMem = oldLatest;
                }
                else if (entry->type == MULU)
                {
                    int oldLatest = schedulerState->latestMult;
                    schedulerState->latestMult = max(oldLatest, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState->latestMult = oldLatest;
                }
            }
            else
            {
                if (entry->type == ADD || entry->type == ADDI || entry->type == SUB || entry->type == MOV)
                {
                    int oldLatest = schedulerState->latestALU1;
                    int oldLatest2 = schedulerState->latestALU2;
                    schedulerState->latestALU1 = max(oldLatest, latestScheduledTime + 1);
                    schedulerState->latestALU2 = max(oldLatest2, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState->latestALU1 = oldLatest;
                    schedulerState->latestALU2 = oldLatest2;
                }
                else if (entry->type == LD || entry->type == ST)
                {
                    int oldLatest = schedulerState->latestMem;
                    schedulerState->latestMem = max(oldLatest, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState->latestMem = oldLatest;
                }
                else if (entry->type == MULU)
                {
                    int oldLatest = schedulerState->latestMult;
                    schedulerState->latestMult = max(oldLatest, latestScheduledTime + 1);
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState->latestMult = oldLatest;
                }
            }
        }

        // if there is a loop
        if (instrs.loopStart != -1)
        {
//-->       // Interloop dependencies
            // handled right after all of BB1 is scheduled
            // also handle the case where the loop is the last instruction
            // if ((instrs.loopEnd == (instrs.size - 1) && table->dependencies[instrs.loopEnd].scheduledTime != -1) || table->dependencies[instrs.loopEnd].scheduledTime != -1 && table->dependencies[instrs.loopEnd + 1].scheduledTime == -1)
            // {
            //     for (int i = instrs.loopStart; i < instrs.loopEnd; i++)
            //     {
            //         DependencyEntry *entry = &table->dependencies[i];

            //         if (entry->loop.size != 0)
            //         {
            //             int latestScheduledTime = -1;
            //             int IDdependsOn = -1;

            //             // look for the latest scheduled time of the instruction(s) it depends on ONLY in the loop (i.e. block == 1)
            //             for (int j = 0; j < entry->loop.size; j++)
            //             {
            //                 if (table->dependencies[entry->loop.list[j].ID - 65].type == MULU)
            //                 {
            //                     if ((table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2) > latestScheduledTime)
            //                     {
            //                         latestScheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2;
            //                         IDdependsOn = entry->loop.list[j].ID - 65;
            //                     }
            //                 }
            //                 else if (table->dependencies[entry->loop.list[j].ID - 65].scheduledTime > latestScheduledTime)
            //                 {
            //                     latestScheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime;
            //                     IDdependsOn = entry->loop.list[j].ID - 65;
            //                 }
            //             }

            //             // first case that needs to be handled: depends on a MULU
            //             if (table->dependencies[IDdependsOn].type == MULU)
            //             {
            //                 // check that the distance between this instruction and the entry is at least 3 cycles

            //                 // distance between the beginning of the loop and the instruction scheduled time
            //                 int x = entry->scheduledTime - table->dependencies[instrs.loopStart].scheduledTime;
            //                 // distance between the end of the loop scheduled time and the instruction it depends on
            //                 int y = table->dependencies[instrs.loopEnd].scheduledTime - table->dependencies[IDdependsOn].scheduledTime;

            //                 // if the distance is less than 3, need to add NOP cycles
            //                 for (int j = 0; j < (3 - (x + y + 1)); j++)
            //                 {
            //                     newVLIW(state);
            //                     VLIW *vliw = &state->bundles.vliw[state->bundles.size - 1];
            //                 }

            //                 // postpone the LOOP or LOOP_PIP instruction to the end of the loop
            //                 vliw = &state->bundles.vliw[table->dependencies[instrs.loopEnd].scheduledTime];
            //                 vliw->br = &NOPinstr;

            //                 vliw = &state->bundles.vliw[state->bundles.size - 1];
            //                 vliw->br = &instrs.instructions[instrs.loopEnd];

            //                 schedulerState->latestBr = state->bundles.size - 1;
            //                 table->dependencies[instrs.loopEnd].scheduledTime = schedulerState->latestBr;
            //             }

            //             // second case to handle: the loop size is 1 
            //             else if (table->dependencies[instrs.loopStart].scheduledTime == table->dependencies[instrs.loopEnd].scheduledTime) 
            //             {
            //                 newVLIW(state);
            //                 VLIW *vliw = &state->bundles.vliw[state->bundles.size - 1];

            //                 // postpone the LOOP or LOOP_PIP instruction to the end of the loop
            //                 vliw = &state->bundles.vliw[table->dependencies[instrs.loopEnd].scheduledTime];
            //                 vliw->br = &NOPinstr;

            //                 vliw = &state->bundles.vliw[state->bundles.size - 1];
            //                 vliw->br = &instrs.instructions[instrs.loopEnd];

            //                 schedulerState->latestBr = state->bundles.size - 1;
            //                 table->dependencies[instrs.loopEnd].scheduledTime = schedulerState->latestBr;
            //             }
            //         }
            //     }
            // }
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
    // rename the loop type to loop.pip
    instrs.instructions[instrs.loopEnd].type = LOOP_PIP; 

    SchedulerState schedulerState;
    schedulerState.latestALU1 = 0;
    schedulerState.latestALU2 = 0;
    schedulerState.latestMult = 0;
    schedulerState.latestMem  = 0;
    schedulerState.latestBr   = 0;
    schedulerState.EC  = -1;
    // schedulerState.P32 = 0;

    // create a new VLIW bundle
    newVLIW(state);

    VLIW *vliw = &state->bundles.vliw[state->bundles.size - 1];

    for (int i = 0; i < table->size; i++)
    {
        DependencyEntry *entry = &table->dependencies[i];

        // Prepare the loop -> insert at the end of BB0: 
        // MOV EC (#stages-1)
        // MOV p32 true
        if (instrs.instructions[i].block == 1 && instrs.instructions[i - 1].block == 0) 
        {
            // insert MOV EC (#stages-1)
            VLIW *vliw = &(state->bundles.vliw[state->bundles.size - 1]);
			printf("stages: %d\n", state->stage);

            if (vliw->alu1->type == NOP)
            {
                printf("Stage = %d\n", state->stage);
                vliw->alu1 = createNewInstruction("mov", 0, -4, -1, -1, state->stage, -1, MOV, 0, false);  // stage will be 0 for now (changed at the end of loop scheduling)   
            }
            else if (vliw->alu2->type == NOP)
            {
                printf("Stage = %d\n", state->stage);
                vliw->alu2 = createNewInstruction("mov", 0, -4, -1, -1, state->stage, -1, MOV, 0, false);  
            }
            else 
            {
                printf("Stage = %d\n", state->stage);
                newVLIW(state);
                vliw = &(state->bundles.vliw[state->bundles.size - 1]);
                vliw->alu1 = createNewInstruction("mov", 0, -4, -1, -1, state->stage, -1, MOV, 0, false); 
            }
            schedulerState.EC = state->bundles.size - 1;  // save the scheduledTime of the MOV EC (#stages-1)

            // insert MOV p32 true
            if (vliw->alu1->type == NOP)
            {
                vliw->alu1 = createNewInstruction("mov", 0, 32, -1, -1, -1, true, MOV, 0, false);  
            }
            else if (vliw->alu2->type == NOP)
            {
                vliw->alu2 = createNewInstruction("mov", 0, 32, -1, -1, -1, true, MOV, 0, false);            
            }
            else 
            {
                newVLIW(state);
                vliw = &(state->bundles.vliw[state->bundles.size - 1]);
                vliw->alu1 = createNewInstruction("mov", 0, 32, -1, -1, -1, true, MOV, 0, false); 
            }
        }

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
            schedulerState.latestMem  = state->bundles.size - 1;
            schedulerState.latestBr   = state->bundles.size - 1;
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
                if (entry->type == ADD || entry->type == ADDI || entry->type == SUB || entry->type == MOV)
                {
                    int oldLatest  = schedulerState.latestALU1;
                    int oldLatest2 = schedulerState.latestALU2;
                    schedulerState.latestALU1 = max(oldLatest, latestScheduledTime + 1);
                    schedulerState.latestALU2 = max(oldLatest2, latestScheduledTime + 1);

                    int minALUTime = (schedulerState.latestALU1 < schedulerState.latestALU2) ? schedulerState.latestALU1 : schedulerState.latestALU2;

                    // for (int j = oldLatest; j < minALUTime-1; j++)
                    // {
                    //     newVLIW(state);
                    //     vliw = &state->bundles.vliw[state->bundles.size - 1];
                    // }
                    // schedule the instruction in the VLIW bundle with the latest scheduled time
                    // vliw->mult = &instrs.instructions[entry->ID-65];
                    // entry->scheduledTime = schedulerState.latestMult;

                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestALU1 = oldLatest;
                    schedulerState.latestALU2 = oldLatest2;
                }
                else if (entry->type == LD || entry->type == ST)
                {
                    printf("Latest mem in schedPip: %d\n", schedulerState.latestMem);
                    int oldLatest = schedulerState.latestMem;
                    schedulerState.latestMem = max(oldLatest, latestScheduledTime + 1);
                    printf("Latest mem in schedPip: %d\n", schedulerState.latestMem);

                    // for (int j = oldLatest; j < schedulerState.latestMem-1; j++)
                    // {
                    //     newVLIW(state);
                    //     vliw = &state->bundles.vliw[state->bundles.size - 1];
                    // }
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestMem = oldLatest;
                }
                else if (entry->type == MULU)
                {
                    int oldLatest = schedulerState.latestMult;
                    schedulerState.latestMult = max(oldLatest, latestScheduledTime + 1);
                    //for (int j = oldLatest; j < schedulerState.latestMult-1; j++)
                    // {
                    //     newVLIW(state);
                    //     vliw = &state->bundles.vliw[state->bundles.size - 1];
                    // }
                    scheduleInstruction(state, entry, &schedulerState);
                    schedulerState.latestMult = oldLatest;
                }

                // new entry must be scheduled AFTER its dependency is resolved (latestScheduledTime + latency) or later
                // only need to do +1 because +2 was already done when computing latestScheduledTime
                // create as many new VLIW bundles as the difference between the old latest and the new latest
            }
            else if (table->dependencies[IDdependsOn].type == LD || table->dependencies[IDdependsOn].type == ST)
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
            // also handle the case where the loop is the last instruction
            // if ((instrs.loopEnd == (instrs.size - 1) && table->dependencies[instrs.loopEnd].scheduledTime != -1) || table->dependencies[instrs.loopEnd].scheduledTime != -1 && table->dependencies[instrs.loopEnd + 1].scheduledTime == -1)
            // {
            //     for (int i = instrs.loopStart; i < instrs.loopEnd; i++)
            //     {
            //         DependencyEntry *entry = &table->dependencies[i];

            //         if (entry->loop.size != 0)
            //         {
            //             int latestScheduledTime = -1;
            //             int IDdependsOn = -1;

            //             //bool doubleDep = false; // flag for cases like  ex. add x3 x3 x3

            //             // look for the latest scheduled time of the instruction(s) it depends on ONLY in the loop (i.e. block == 1)
            //             for (int j = 0; j < entry->loop.size; j++)
            //             {
            //                 if (table->dependencies[entry->loop.list[j].ID - 65].type == MULU)
            //                 {
            //                     if ((table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2) > latestScheduledTime)
            //                     {
            //                         latestScheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime + 2;
            //                         IDdependsOn = entry->loop.list[j].ID - 65;
            //                     }
            //                 }
            //                 else if (table->dependencies[entry->loop.list[j].ID - 65].scheduledTime > latestScheduledTime)
            //                 {
            //                     latestScheduledTime = table->dependencies[entry->loop.list[j].ID - 65].scheduledTime;
            //                     IDdependsOn = entry->loop.list[j].ID - 65;
            //                 }
            //             }

            //             // first case that needs to be handled: depends on a MULU
            //             if (table->dependencies[IDdependsOn].type == MULU)
            //             {
            //                 // check that the distance between this instruction and the entry is at least 3 cycles

            //                 // distance between the beginning of the loop and the instruction scheduled time
            //                 int x = entry->scheduledTime - table->dependencies[instrs.loopStart].scheduledTime;
            //                 // distance between the end of the loop scheduled time and the instruction it depends on
            //                 int y = table->dependencies[instrs.loopEnd].scheduledTime - table->dependencies[IDdependsOn].scheduledTime;

            //                 // if the distance is less than 3, need to add NOP cycles
            //                 for (int j = 0; j < (3 - (x + y + 1)); j++)
            //                 {
            //                     newVLIW(state);
            //                     VLIW *vliw = &state->bundles.vliw[state->bundles.size - 1];
            //                 }

            //                 // postpone the LOOP or LOOP_PIP instruction to the end of the loop
            //                 vliw = &state->bundles.vliw[table->dependencies[instrs.loopEnd].scheduledTime];
            //                 vliw->br = &NOPinstr;

            //                 vliw = &state->bundles.vliw[state->bundles.size - 1];
            //                 vliw->br = &instrs.instructions[instrs.loopEnd];

            //                 schedulerState.latestBr = state->bundles.size - 1;
            //                 table->dependencies[instrs.loopEnd].scheduledTime = schedulerState.latestBr;
            //             }

            //             // second case to handle: the loop size is 1 
            //             else if (table->dependencies[instrs.loopStart].scheduledTime == table->dependencies[instrs.loopEnd].scheduledTime) 
            //             {
            //                 newVLIW(state);
            //                 VLIW *vliw = &state->bundles.vliw[state->bundles.size - 1];

            //                 // postpone the LOOP or LOOP_PIP instruction to the end of the loop
            //                 vliw = &state->bundles.vliw[table->dependencies[instrs.loopEnd].scheduledTime];
            //                 vliw->br = &NOPinstr;

            //                 vliw = &state->bundles.vliw[state->bundles.size - 1];
            //                 vliw->br = &instrs.instructions[instrs.loopEnd];

            //                 schedulerState.latestBr = state->bundles.size - 1;
            //                 table->dependencies[instrs.loopEnd].scheduledTime = schedulerState.latestBr;
            //             }
            //         }
            //     }
            // }
        }

        if (schedulerState.EC != -1)
        {
            int changed = 0;

            do {
                printf("table->dependencies[instrs.loopEnd].scheduledTime + 1: %d\n", table->dependencies[instrs.loopEnd].scheduledTime + 1);
                printf("table->dependencies[instrs.loopStart].scheduledTime: %d\n", table->dependencies[instrs.loopStart].scheduledTime);
                printf("II: %d\n", state->II);
                // Once the loop is scheduled, compute the number of loop stages
                state->stage = floor((table->dependencies[instrs.loopEnd].scheduledTime +1 - table->dependencies[instrs.loopStart].scheduledTime) / state->II);
                printf("Number of stages: %d\n", state->stage);

                // check if the II needs to be updated
                changed = checkInterloopDependencies(table, state);
                printf("Changed: %d\n", changed);
                printf("II: %d\n", state->II);
                printf("\n");

                if (changed)
                {
                    // reschedule the instructions
                    rescheduleLoop(state, table, &schedulerState);
                }
            } while (changed);

            // if the number of stages has changed, update the EC value
            VLIW *vliw = &state->bundles.vliw[schedulerState.EC];
        
            if (vliw->alu1->dest == -4)
            {
                vliw->alu1->imm = state->stage -1;
                printf("EC value ALU1: %d", vliw->alu1->imm); 
            } 
            else 
            {
                vliw->alu2->imm = state->stage -1; 
                printf("EC value ALU2: %d\n", vliw->alu2->imm);
            }
        }

    }
}

/**
 * @brief Schedule the instructions in the VLIW bundles based on whether they have a loop or not.
 * @param state Pointer to the processor state.
 * @param table Pointer to the DependencyTable.
 */
void scheduler(ProcessorState *state, DependencyTable *table)
{
    // Schedule the instructions in the VLIW bundles
    if (instrs.loopStart == -1)
    {
        scheduleInstructions(state, table);
        registerAllocation(state, table);
    }
    else
    {
        printf("Scheduling with LOOP_PIP\n");
        scheduleInstructionsPip(state, table);
        printf("Register Allocation\n");
        registerAllocationPip(state, table);
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

/**
 * @brief Empty the Instruction Set.
 */
void emptyInstructions()
{
    instrs.size = 0;
    instrs.loopEnd = -1;
    instrs.loopStart = -1;
    free(instrs.instructions);
    instrs.instructions = NULL;
}

/**
 * @brief Function to write output of an instruction in a JSON file.
 * @param inst The instruction to convert to a string.
 * @return const char* The string representation of the instruction.
 */
const char *instructionToString(InstructionEntry *inst)
{
    char *str = malloc(200);
    
    switch (inst->type)
    {
        case ADDI:
            if (inst->cycle != -1)
            {
                sprintf(str, "(p%d) addi x%d, x%d, %d", inst->cycle, inst->dest, inst->src1, inst->imm);
            }
            else
            {
                sprintf(str, " addi x%d, x%d, %d", inst->dest, inst->src1, inst->imm);
            }
           
            break;
        case ADD:
            if (inst->cycle != -1)
            {
                sprintf(str, "(p%d) add x%d, x%d, x%d", inst->cycle, inst->dest, inst->src1, inst->src2);
            }
            else
            {
                sprintf(str, " add x%d, x%d, x%d", inst->dest, inst->src1, inst->src2);
            }
            break;
        case SUB:
            if (inst->cycle != -1)
            {
                sprintf(str, "(p%d) sub x%d, x%d, x%d", inst->cycle, inst->dest, inst->src1, inst->src2);
            }
            else
            {
                sprintf(str, " sub x%d, x%d, x%d", inst->dest, inst->src1, inst->src2);
            }
            break;
        case MOV:  // TODO MOV inside loop ?? if yes, (p%d)
            switch (inst->dest)
            {
            case -3:
                sprintf(str, " mov LC, %d", inst->imm);
                break;
            case -4:
                sprintf(str, " mov EC, %d", inst->imm);
                break;
            default:
                if (inst->predicate >= 0)
                {
                    inst->predicate ? sprintf(str, " mov p%d, true", inst->dest) : sprintf(str, "mov p%d,false", inst->dest);
                }
                else if (inst->imm == -1)
                {
                    sprintf(str, " mov x%d, x%d", inst->dest, inst->src1);
                }
                else
                {
                    sprintf(str, " mov x%d, %d", inst->dest, inst->imm);
                }
                break;
            }
            break;
        case LD:
            if (inst->cycle != -1)
            {
                sprintf(str, "(p%d) ld x%d, %d(x%d)", inst->cycle, inst->dest, inst->imm, inst->src1);
            }
            else
            {
                sprintf(str, " ld x%d, %d(x%d)", inst->dest, inst->imm, inst->src1);
            }
            break;
        case ST:
            if (inst->cycle != -1)
            {
                sprintf(str, "(p%d) st x%d, %d(x%d)", inst->cycle, inst->src2, inst->imm, inst->src1);
            }
            else
            {
                sprintf(str, " st x%d, %d(x%d)", inst->src2, inst->imm, inst->src1);
            }
            break;
        case MULU:
            if (inst->cycle != -1)
            {
                sprintf(str, "(p%d) mulu x%d, x%d, x%d", inst->cycle, inst->dest, inst->src1, inst->src2);
            }
            else
            {
                sprintf(str, " mulu x%d, x%d, x%d", inst->dest, inst->src1, inst->src2);
            }
            break;

        case LOOP:
            sprintf(str, " loop %d", inst->imm);
            break;

        case LOOP_PIP:
            sprintf(str, " loop.pip %d", inst->imm);
            break;

        default:
            sprintf(str, " nop");
            break;
    }
    return str;

}


/**
 * @brief Function to write the VLIW bundles to a JSON file.
 * @param bundles The VLIW bundles to write.
 * @param filename The name of the file to write to.
 */
void writeVLIWToJson(VLIWBundles *bundles, const char *filename)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        perror("Failed to open file");
        return;
    }
    char* alu1;
    char* alu2;
    char* mult;
    char* mem;
    char* br;

    fflush(stdout);
    fprintf(file, "[\n");
    for (int i = 0; i < bundles->size; i++)
    {
        fprintf(file, "  [");
        alu1 = instructionToString(bundles->vliw[i].alu1);
        alu2 = instructionToString(bundles->vliw[i].alu2);
        mult = instructionToString(bundles->vliw[i].mult);
        mem = instructionToString(bundles->vliw[i].mem);
        br = instructionToString(bundles->vliw[i].br);

        fprintf(file, "\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"", alu1, alu2, mult, mem, br);
       

        if (i < bundles->size - 1)
        {
            fprintf(file, "],\n");
        }
        else
        {
            fprintf(file, "]\n");
        }
    }
    fprintf(file, "]\n");
    fclose(file);
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