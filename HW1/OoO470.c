#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "lib/cJSON.h"
#include "lib/cJSON_Utils.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define REGS 64
#define ENTRY 32
#define INSTR 4
#define OPCODE 5
#define min(x, y) ((x) < (y) ? (x) : (y))

// Structure for parsing JSON entry
typedef struct
{
    char opcode[OPCODE]; // 4 characters + null terminator
    int dest;
    int src1;
    int src2;
} InstructionEntry;

// Structure for parsing JSON
typedef struct
{
    InstructionEntry *instructions;
    size_t size;
} Instruction;

Instruction instrs = {NULL, 0};

// Program Counter unsigned integer pointing to the next instruction to fetch.
unsigned int PC = 0;

// Physical Register File
unsigned long PhysRegFile[REGS]; // 64 registers of 64 bits each

// Decoded Instruction Register
/*
unsigned int * DIR; // array that buffers instructions that have been decoded but have not been renamed and dispatched yet
unsigned int DIRSize = 0; // size of the DIR*/
struct
{
    unsigned int *DIRarray; // array that buffers instructions that have been decoded but have not been renamed and dispatched yet
    unsigned int DIRSize;   // size of the DIR
} DIR;

// popDIR and realloc DIRarray
int popDIR()
{
    if (DIR.DIRSize == 0)
    {
        return -1;
    }
    for (size_t i = 0; i < DIR.DIRSize - 1; i++)
    {
        DIR.DIRarray[i] = DIR.DIRarray[i + 1];
    }
    DIR.DIRarray = realloc(DIR.DIRarray, (DIR.DIRSize - 1) * sizeof(unsigned int));
    DIR.DIRSize -= 1;
    return 0;
}

int pushDIR(unsigned int instr)
{
    DIR.DIRarray = realloc(DIR.DIRarray, (DIR.DIRSize + 1) * sizeof(unsigned int));
    DIR.DIRarray[DIR.DIRSize] = instr;
    DIR.DIRSize += 1;
    return 0;
}

// Exception Flag
bool exception = false;

// Exception PC
unsigned int ePC = 0;

bool backPressureRDS = false;

// Register Map Table
unsigned int RegMapTable[ENTRY] = { // On initialization, all architectural registers are mapped to physical registers with the same id
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31}; // array that maps architectural register names to physical register names
// 32 architectural registers, 64 physical registers

// Free List
unsigned int *FreeList[ENTRY] = {
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    62, 63}; // array that keeps track of the physical registers that are free
             // array that keeps track of the physical registers that are free
// on initialization 32-63 are free

// Busy Bit Table
bool BusyBitTable[REGS] = {false}; // whether the value of a specific physical register will be generated from the Execution stage

// Entry in the Active List
typedef struct
{
    bool Done;
    bool Exception;
    int LogicalDestination;
    int OldDestination;
    int PC;
} ActiveListEntry;
int PopFreeList()
{
    int reg = FreeList[0];
    for (size_t i = 0; i < ENTRY - 1; i++)
    {
        FreeList[i] = FreeList[i + 1];
        // printf("%d <-%d\n",FreeList[i],FreeList[i+1]);
    }
    FreeList[ENTRY - 1] = -1;
    return reg;
}

int getFreeReg()
{

    return PopFreeList();
}

int PushFreeList(int reg)
{
    if (FreeList[ENTRY - 1] == -1)
    {
        return -1;
    }
    for (size_t i = 0; i < ENTRY; i++)
    {
        FreeList[i] = FreeList[i + 1];
    }
    FreeList[0] = reg;
    return 0;
}

// Active List
// instructions that have been dispatched but have not yet completed
// renamed instruction
/*
ActiveListEntry ActiveList[ENTRY]; */

struct
{
    ActiveListEntry ALarray[ENTRY];
    int ALSize;
} ActiveList;

// Entry in Integer Queue
typedef struct
{
    int DestRegister;
    bool OpAIsReady;
    int OpARegTag; // for cheking forwarding
    int OpAValue;
    bool OpBIsReady;
    int OpBRegTag;
    int OpBValue;
    char OpCode[OPCODE]; // 4 characters + null terminator
    int PC;
} IntegerQueueEntry;

typedef struct
{
    IntegerQueueEntry instr;
    unsigned int ActiveListIndex;
} ALUEntry;

ALUEntry ALU1[INSTR];
ALUEntry ALU2[INSTR];

// Integer Queue
/*
IntegerQueueEntry IntegerQueue[ENTRY]; */
// always 32 entries myx but can be less, need to check if it is full
struct
{
    IntegerQueueEntry IQarray[ENTRY];
    int IQSize;
} IntegerQueue;

int parser(char *file_name)
{

    // Open the JSON file
    FILE *file = fopen(file_name, "r");
    if (file == NULL)
    {
        // printf("Failed to open the file.\n");
        return 1;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file); // Go back to the beginning of the file
    // printf("File size: %ld\n", file_size);

    // Allocate memory to store the file contents
    char *json_data = (char *)malloc(file_size + 1);
    if (json_data == NULL)
    {
        // printf("Memory allocation failed.\n");
        fclose(file);
        return 1;
    }
    // printf("Memory allocated.\n");

    // Read the file contents into the allocated memory
    fread(json_data, 1, file_size, file);
    json_data[file_size] = '\0'; // Null-terminate the string

    // Close the file
    fclose(file);

    // Parse the JSON data
    cJSON *root = cJSON_Parse(json_data);
    if (root == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            // printf("Error before: %s\n", error_ptr);
        }
        cJSON_free(json_data); // Free memory
        return 1;
    }
    // printf("JSON parsed.\n");

    // Iterate over the array and print each string
    cJSON *instruction;

    size_t array_size = cJSON_GetArraySize(root);

    if (array_size == 0)
    {
        // printf("Error: Empty or invalid JSON array.\n");
        cJSON_Delete(root);
        free(json_data);
        return 1;
    }
    // printf("Array size: %ld\n", array_size);

    instrs.instructions = (InstructionEntry *)malloc(array_size * sizeof(InstructionEntry));
    if (instrs.instructions == NULL)
    {
        // printf("Instruction memory allocation failed.\n");
        cJSON_Delete(root);
        free(json_data);
        return 1;
    }
    instrs.size = 0;
    // printf("Memory allocated for instructions.\n");

    cJSON_ArrayForEach(instruction, root)
    {
        // printf("%s\n", cJSON_Print(instruction));
        //  printf("%s\n", instruction->valuestring);

        // Parse at whitespace and eliminate ',' and store in InstructionEntry

        // Copy the instruction string since strtok modifies the input
        char *instruction_copy = strdup(instruction->valuestring);

        // Tokenize the instruction string
        char *token = strtok(instruction_copy, " ");

        if (token != NULL)
        {

            strcpy(instrs.instructions[instrs.size].opcode, token); // already null-terminated

            token = strtok(NULL, ",");
            if (token != NULL)
            {
                // printf("Token: %s\n", token);
                // remove the 'x' from the string
                token = token + 1;
                // printf("Token: %s\n", token);
                sscanf(token, "%d", &instrs.instructions[instrs.size].dest);
                // printf("Dest: %d\n", instrs.instructions[instrs.size].dest);

                token = strtok(NULL, ",");
                if (token != NULL)
                {
                    // printf("Token: %s\n", token);
                    // remove the 'x' from the string
                    token = token + 2;
                    // printf("Token: %s\n", token);
                    sscanf(token, "%d", &instrs.instructions[instrs.size].src1);
                    // printf("Src1: %d\n", instrs.instructions[instrs.size].src1);
                    token = strtok(NULL, "\0");
                    // printf("idk","ok");
                    if (token != NULL)
                    { // check if src2 starts by 'x' if so remove it
                        if (token[0] == 'x')
                        {
                            token = token + 2;
                            //    printf("Token: %s\n", token);
                            sscanf(token, "%d", &instrs.instructions[instrs.size].src2);
                        }
                        else
                        {
                            sscanf(token, "%d", &instrs.instructions[instrs.size].src2);
                        }
                    }
                }
            }
        }
        // printf("Token: %s\n", token);
        instrs.size += 1;
    }

    // Free memory
    cJSON_Delete(root);
    free(json_data);

    // // Initialize the Free List
    // FreeList = (unsigned int *)malloc((REGS - 32) * sizeof(unsigned int));
    // for (int i = 0; i < (REGS - 32); i++) {
    //     FreeList[i] = 32 + i;
    // }

    // // Fetch & Decode
    // FetchAndDecode();

    // // Rename
    // Rename();

    // // Dispatch
    // Dispatch();

    // // Issue
    // Issue();

    // // Execute
    // Execute();

    // // Write Result
    // WriteResult();

    // // Commit
    // Commit();

    return 0;
}

int Commit()
{
    return 0;
}

bool isOpBusy(int reg)
{
    return BusyBitTable[reg];
}

typedef struct
{
    int reg;
    int value;
} forwardingTableEntry;

struct
{
    forwardingTableEntry table[ENTRY];
    int size;
} forwardingTable;

int forwardable(int reg)
{
    for (int i = 0; i < INSTR; i++)
    {
        if (forwardingTable.table[i].reg == reg)
        {
            return i;
        }
    }
    return -1;
}

// Fetch & Decode
void FetchAndDecode()
{
    // Fetch MAX 4 instructions from memory

    // Decode the instructions
    // get first 4 instructions from buffer
    // read JSON file until the end

    /*
    When the Commit stage
     indicates that an exception is detected, the PC is set to 0x10000
     */
    // TODO: create Commit Stage
    if (Commit() < 0)
    {
        // set at 0x10000
        PC = 0x10000;
    }
    // Check Back Pressure
    if (backPressureRDS)
    {
        return;
    }

    int index = min(instrs.size - PC, INSTR); // get the first 4 instructions
    if (index <= 0)
    {
        return;
    }
    for (size_t i = 0; i < index; i++)
    {

        // Buffer the decoded instructions in the DIR
        pushDIR(PC);
        PC += 1;
    }
}

// Rename

void RDS()
{

    if (ActiveList.ALSize == ENTRY || IntegerQueue.IQSize == ENTRY)
    {
        backPressureRDS = true;
        return;
    }

    else
    {
        backPressureRDS = false;
    }

    if (DIR.DIRSize == 0)
    {
        return;
    }

    // Rename the instructions

    
    unsigned int index = min(DIR.DIRSize, INSTR);
    for (int i = 0; i < index; i++)
    {
    
        int newReg = getFreeReg();
        if (newReg < 0)
        {
            return;
        }
        // printf("newReg: %d\n", newReg);
        // printf("DIR.DIRarray[i]: %d\n", DIR.DIRarray[i]);
        //  make it busy

        unsigned int oldReg = RegMapTable[instrs.instructions[i].dest];
        // map the logical destination to the physical register

        // add the instruction to the Active List
        unsigned int currentPc = popDIR();
        ActiveList.ALarray[ActiveList.ALSize].LogicalDestination = instrs.instructions[i].dest;
        ActiveList.ALarray[ActiveList.ALSize].OldDestination = oldReg,
        ActiveList.ALarray[ActiveList.ALSize].PC = currentPc;
        ActiveList.ALarray[ActiveList.ALSize].Done = false;
        ActiveList.ALSize += 1;
        // add the instruction to the Integer Queue
        IntegerQueue.IQarray[IntegerQueue.IQSize].DestRegister = newReg;
        IntegerQueue.IQarray[IntegerQueue.IQSize].PC = currentPc;
        const char *tempOpCode = instrs.instructions[i].opcode;
        strcpy(IntegerQueue.IQarray[IntegerQueue.IQSize].OpCode, tempOpCode);

        // we want to check if the source registers are ready
        // if they are ready, we want to forward the value to the Integer Queue
        // if opcode is "addi" then we only need to check if src1 is ready
        // and we give the value of src2 to IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue
        // if opcode is not "addi" then we need to check if both src1 and src2 are ready
        // and we give the value of src1 to IntegerQueue.IQarray[IntegerQueue.IQSize].OpAValue
        int forwardIndexA = forwardable(RegMapTable[instrs.instructions[i].src1]);

        if (isOpBusy(RegMapTable[instrs.instructions[i].src1]))
        {
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAIsReady = false;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpARegTag = RegMapTable[instrs.instructions[i].src1];
        }
        else if (forwardIndexA >= 0)
        {
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAIsReady = true;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAValue = forwardingTable.table[forwardIndexA].value;
        }
        else
        {
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAIsReady = true;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAValue = PhysRegFile[RegMapTable[instrs.instructions[i].src1]];
        }

        if (strcmp(instrs.instructions[i].opcode, "addi") == 0)
        {
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = true;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue = instrs.instructions[i].src2;
        }
        else
        {
            int forwardIndexB = forwardable(RegMapTable[instrs.instructions[i].src2]);

            if (isOpBusy(RegMapTable[instrs.instructions[i].src2]))
            {
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = false;
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBRegTag = RegMapTable[instrs.instructions[i].src2];
            }
            else if (forwardIndexB >= 0)
            {
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = true;
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue = forwardingTable.table[forwardIndexB].value;
            }
            else
            {
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = true;
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue = PhysRegFile[RegMapTable[instrs.instructions[i].src2]];
            }
        }
    
        RegMapTable[instrs.instructions[i].dest] = newReg;
        BusyBitTable[newReg] = true;
        IntegerQueue.IQSize += 1;

    }
    // Clear the DIR

    return;
}

void Issue()
{
    // scan for the 4 available entries in the Integer Queue
    int index = min(IntegerQueue.IQSize, ENTRY);
    if (index <= 0)
    {
        return;
    }

    struct
    {
        IntegerQueueEntry ilist[INSTR];
        size_t size;
    } tempList;

    for (size_t i = index; i >= 0; i--)
    {
        if (IntegerQueue.IQarray[i].OpAIsReady && IntegerQueue.IQarray[i].OpBIsReady)
        {
            tempList.ilist[tempList.size] = IntegerQueue.IQarray[i];
            tempList.size += 1;
        }
        if (tempList.size == INSTR)
        {
            break;
        }
    }
}

void Execute()
{
    int temp;
    for (int i = 0; i < INSTR; i++)
    {
        if (strcmp((ALU2[i]).instr.OpCode, "addi") == 0)
        {
            temp = (ALU2[i]).instr.OpAValue + (ALU2[i]).instr.OpBValue;
        }
        else if (strcmp((ALU2[i]).instr.OpCode, "add") == 0)
        {
            temp = (ALU2[i]).instr.OpAValue + (ALU2[i]).instr.OpBValue;
        }
        else if (strcmp((ALU2[i]).instr.OpCode, "sub") == 0)
        {
            temp = (ALU2[i]).instr.OpAValue - (ALU2[i]).instr.OpBValue;
        }
        else if (strcmp((ALU2[i]).instr.OpCode, "mulu") == 0)
        {
            temp = (ALU2[i]).instr.OpAValue * (ALU2[i]).instr.OpBValue;
        }
        else if (strcmp((ALU2[i]).instr.OpCode, "divu") == 0)
        {
            temp = (ALU2[i]).instr.OpAValue / (ALU2[i]).instr.OpBValue;
        }
        else if (strcmp((ALU2[i]).instr.OpCode, "remu") == 0)
        {
            temp = (ALU2[i]).instr.OpAValue % (ALU2[i]).instr.OpBValue;
        }
        else
        {
            temp = 0;
        }
        forwardingTable.table[i].reg = (ALU2[i]).instr.DestRegister;
        forwardingTable.table[i].value = temp;
        ALU2[i] = ALU1[i];

        return;
    }
}

void showDIR()
{
    printf("DIR\n");
    for (size_t i = 0; i < DIR.DIRSize; i++)
    {
        printf("%d\n", DIR.DIRarray[i]);
    }
}
void showActiveList()
{
    printf("ActiveList\n");
    for (size_t i = 0; i < ActiveList.ALSize; i++)
    {
        printf("%d %d %d\n", ActiveList.ALarray[i].LogicalDestination, ActiveList.ALarray[i].OldDestination, ActiveList.ALarray[i].PC);
    }
}

void showIntegerQueue()
{
    printf("IntegerQueue\n");
    for (size_t i = 0; i < IntegerQueue.IQSize; i++)
    {
        printf("%d %d %d %d %d %d %d %s %d\n", IntegerQueue.IQarray[i].DestRegister, IntegerQueue.IQarray[i].OpAIsReady, IntegerQueue.IQarray[i].OpARegTag, IntegerQueue.IQarray[i].OpAValue, IntegerQueue.IQarray[i].OpBRegTag, IntegerQueue.IQarray[i].OpBIsReady, IntegerQueue.IQarray[i].OpBValue, IntegerQueue.IQarray[i].OpCode, IntegerQueue.IQarray[i].PC);
    }
}

void showInstruction()
{
    printf("Instructions\n");
    char *temp;
    for (size_t i = 0; i < instrs.size; i++)
    {
        if (strcmp(instrs.instructions[i].opcode, "addi") != 0)
        {
            temp = "x";
        }
        else
        {
            temp = "";
        }

        printf("%s x%d, x%d, %s%d\n", instrs.instructions[i].opcode,
               instrs.instructions[i].dest, instrs.instructions[i].src1,
               temp,
               instrs.instructions[i].src2);
    }
}

void showBusyBitTable()
{
    printf("BusyBitTable\n");
    for (size_t i = 0; i < REGS; i++)
    {
        printf("%d ", BusyBitTable[i]);
    }
    printf("\n");
}

void showRegMapTable()
{
    printf("RegMapTable\n");
    for (size_t i = 0; i < ENTRY; i++)
    {
        printf("%d ->%d\n", i, RegMapTable[i]);
    }
}

void showFreeList()
{
    printf("FreeList\n");
    for (size_t i = 0; i < ENTRY; i++)
    {
        printf("%d ", FreeList[i]);
    }
    printf("\n");
}

void showPhysRegFile()
{
    printf("PhysRegFile\n");
    for (size_t i = 0; i < REGS; i++)
    {
        printf("%lu ", PhysRegFile[i]);
    }
    printf("\n");
}
void showBp()
{
    printf("BackPressure: %d\n", backPressureRDS);
}