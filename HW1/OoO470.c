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
struct
{
    unsigned int *DIRarray; // array that buffers instructions that have been decoded but have not been renamed and dispatched yet
    unsigned int DIRSize;   // size of the DIR
} DIR;

// popDIR and realloc DIRarray
int popDIR()
{
    int temp = DIR.DIRarray[0]; // TODO LinkedList for complexity O(1)
    if (DIR.DIRSize == 0)
        return -1; // if the DIR is empty, return -1

    for (size_t i = 0; i < DIR.DIRSize - 1; i++)
    {
        DIR.DIRarray[i] = DIR.DIRarray[i + 1];
    }
    if (DIR.DIRSize > 1)
    {
        DIR.DIRarray = realloc(DIR.DIRarray, (DIR.DIRSize - 1) * sizeof(unsigned int));
        if (DIR.DIRarray == NULL)
            return -1; // if realloc fails, return -1
    }
    else
    {
        free(DIR.DIRarray);
        DIR.DIRarray = NULL;
    }

    DIR.DIRSize -= 1;
    return temp;
}

int pushDIR(unsigned int instr)
{
    DIR.DIRarray = realloc(DIR.DIRarray, (DIR.DIRSize + 1) * sizeof(unsigned int));
    if (DIR.DIRarray == NULL)
        return -1; // if realloc fails, return -1

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

// // Free List
// unsigned int *FreeList[ENTRY] = {  // TODO why * ?
//     32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
//     42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
//     52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
//     62, 63 }; // array that keeps track of the physical registers that are free
// // on initialization 32-63 are free

// // Free List
// unsigned int FreeList[REGS] = {
//     32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
//     42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
//     52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
//     62, 63, -1, -1, -1, -1, -1, -1, -1, -1,
//     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//     -1, -1, -1, -1}; // array that keeps track of the physical registers that are free
// // on initialization 32-63 are free

// Define a struct for a node in the double linked list
typedef struct FreeListNode
{
    int value;                 // The value of the register
    struct FreeListNode *prev; // Pointer to the previous node
    struct FreeListNode *next; // Pointer to the next node
} FreeListNode;

// FreeList
typedef struct
{
    FreeListNode *head; // Pointer to the head of the list
    FreeListNode *tail; // Pointer to the tail of the list
} FreeList;

// Function to initialize the double linked list
FreeList initFreeList()
{
    FreeList list;
    list.head = NULL;
    list.tail = NULL;

    // Create nodes for each register and link them together
    for (int i = (REGS - 1); i >= 32; i--)
    {
        FreeListNode *newNode = (FreeListNode *)malloc(sizeof(FreeListNode));
        newNode->value = i;
        newNode->prev = NULL;
        newNode->next = list.head;
        if (list.head != NULL)
        {
            (list.head)->prev = newNode; // Link the previous head to the new node
        }
        list.head = newNode; // Set the new node as the head
    }

    return list;
}

FreeList freeList; // Initialize the FreeList

// Function to pop the first element of the list
int popFreeList(FreeList *list) {
    if (list->head == NULL) {
        return -1; // If the list is empty, return -1
    }
    int value = list->head->value; // Get the value of the head
    FreeListNode *temp = list->head; // Store the head in a temporary variable
    list->head = list->head->next; // Move the head to the next node
    if (list->head != NULL) {
        list->head->prev = NULL; // Set the previous node of the new head to NULL
    } else {
        list->tail = NULL; // If the list is empty, set the tail to NULL
    }
    free(temp);   // Free the memory of the old head
    return value; // Return the value of the old head
}

// Function to push a value to the end of the list
void pushFreeList(FreeList *list, int value) {
    FreeListNode *newNode = (FreeListNode *)malloc(sizeof(FreeListNode)); // Create a new node
    newNode->value = value; // Set the value of the new node
    newNode->next = NULL; // Set the next node to NULL
    if (list->tail != NULL) {
        list->tail->next = newNode; // Link the old tail to the new node
        newNode->prev = list->tail; // Link the new node to the old tail
    } else {
        list->head = newNode; // If the list is empty, set the new node as the head
        newNode->prev = NULL; // Set the previous node to NULL
    }
    list->tail = newNode; // Set the new node as the tail
}

// Busy Bit Table
bool BusyBitTable[REGS] = {false}; // whether the value of a specific physical register will be generated from the Execution stage

// Entry in the Active List
typedef struct
{
    bool Done;
    bool Exception;
    int LogicalDestination; // the architectural register that the instruction writes to
    int OldDestination;
    int PC;
} ActiveListEntry;

// Active List
// instructions that have been dispatched but have not yet completed, renamed instruction
struct {
    ActiveListEntry ALarray[ENTRY];
    int ALSize;
} ActiveList;

typedef struct
{
    int reg;
    freeListEntry *next;
    freeListEntry *prev;
} freeListEntry;

int PopFreeList()
{
    for (size_t i = 0; i < REGS; ++i)
    {
        if (FreeList[i] != -1)
        {
            unsigned int poppedValue = FreeList[i];
            FreeList[i] = -1;
            return poppedValue;
        }
    }
    return -1; // If all elements are -1, FreeList is empty
}

// int PopFreeList() {
//     int reg = FreeList[0];
//     for (size_t i = 0; i < ENTRY - 1; i++) {
//         FreeList[i] = FreeList[i + 1];
//         // printf("%d <-%d\n",FreeList[i],FreeList[i+1]);
//     }
//     FreeList[ENTRY - 1] = -1;
//     return reg;
// }

/*
    Push a register to the beginning of the Free List
    If the Free List is full, return -1
    Else, return 0
*/
int PushFreeList(unsigned int reg)
{
    printf("Free list reg: %d\n", reg);
    for (size_t i = 0; i < REGS; ++i)
    {
        if (FreeList[i] == -1)
        {
            FreeList[i] = reg;
            return 0;
        }
    }
    return -1; // If no element is -1, FreeList is full
}

// int PushFreeList(int reg) {

//     if (FreeList[ENTRY - 1] == -1) return -1;  // TODO if (FreeList[ENTRY - 1] == -1) never -1

//     for (size_t i = 0; i < ENTRY; i++) {
//         FreeList[i] = FreeList[i + 1];
//     }
//     FreeList[0] = reg;
//     return 0;
// }

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

// Integer Queue
// always 32 entries myx but can be less, need to check if it is full
struct
{
    IntegerQueueEntry IQarray[ENTRY];
    int IQSize;
} IntegerQueue;

IntegerQueueEntry createNop()
{
    IntegerQueueEntry temp = {-1, true, -1, 0, true, -1, 0, "nop", -1};
    return temp;
}

IntegerQueueEntry copyIQE(IntegerQueueEntry entry)
{
    IntegerQueueEntry temp;
    temp.DestRegister = entry.DestRegister;
    temp.OpAIsReady = entry.OpAIsReady;
    temp.OpARegTag = entry.OpARegTag;
    temp.OpAValue = entry.OpAValue;
    temp.OpBIsReady = entry.OpBIsReady;
    temp.OpBRegTag = entry.OpBRegTag;
    temp.OpBValue = entry.OpBValue;
    strcpy(temp.OpCode, entry.OpCode);
    temp.PC = entry.PC;

    return temp;
}

/*
    Pop the oldest instruction that has both operands ready
    If no such instruction exists, return a NOP
*/
IntegerQueueEntry popReadyIQE()
{
    // we are looking for the oldest instruction that has both operands ready
    // the oldest are the nearest to 0 in the queue
    for (size_t i = 0; i < IntegerQueue.IQSize; i++)
    {

        if (IntegerQueue.IQarray[i].OpAIsReady && IntegerQueue.IQarray[i].OpBIsReady)
        {
            IntegerQueueEntry temp = IntegerQueue.IQarray[i];

            for (size_t j = i; j < IntegerQueue.IQSize - 1; j++)
            {
                IntegerQueue.IQarray[j] = IntegerQueue.IQarray[j + 1];
            }
            IntegerQueue.IQSize -= 1;
            return temp;
        }
    }
    IntegerQueueEntry temp = createNop();
    return temp;
}

typedef struct
{
    IntegerQueueEntry instr;
} ALUEntry;

ALUEntry ALU1[INSTR];
ALUEntry ALU2[INSTR]; // TODO 4 ALUs not 2 => max 4 instructions

void initALU()
{
    for (size_t i = 0; i < INSTR; i++)
    {
        ALU1[i].instr = createNop();
        ALU2[i].instr = createNop();
    }
}

/*
    Parser for JSON file
*/
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

bool isOpBusy(int reg)
{
    return BusyBitTable[reg];
}

// Forwarding Table
typedef struct
{
    int reg;   // the physical register that the value is forwarded to
    int value; // the value that is forwarded
} forwardingTableEntry;

struct
{
    forwardingTableEntry table[INSTR];
    int size;
} forwardingTable;

void initForwardingTable()
{
    for (size_t i = 0; i < INSTR; i++)
    {
        forwardingTable.table[i].reg = -1;
        forwardingTable.table[i].value = 0;
    }
}

/*
    Check if the register is in the forwarding table
    If it is, return the index
    If it is not, return -1
*/
int forwardable(int reg)
{
    for (int i = 0; i < INSTR; i++)
    { // 4 instructions MAX in the forwarding table because 4 ALUs
        if (forwardingTable.table[i].reg == reg)
            return i; // if the register is in the forwarding table, return the index
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
    if (exception)
    {
        // set PC at 0x10000
        PC = 0x10000;
        // clear Decode Instruction Register
        size_t size = DIR.DIRSize;
        for (size_t i = 0; i < size; i++)
        {
            popDIR();
        }
        if (DIR.DIRSize != 0)
        {
            printf("Failed to pop DIR; not empty...\n");
        }
    }
    // Check Back Pressure
    if (backPressureRDS)
        return;

    int index = min(instrs.size - PC, INSTR); // get the first 4 instructions
    if (index <= 0)
        return;

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
        backPressureRDS = false;

    if (DIR.DIRSize == 0)
        return;

    // Rename the instructions

    unsigned int index = min(DIR.DIRSize, INSTR);
    for (int i = 0; i < index; i++)
    {
        int newReg = popFreeList();
        if (newReg < 0)
            return;
        // printf("newReg: %d\n", newReg);
        // printf("DIR.DIRarray[i]: %d\n", DIR.DIRarray[i]);
        //  make it busy

        // map the logical destination to the physical register

        // add the instruction to the Active List

        unsigned int currentPc = popDIR();

        ActiveList.ALarray[ActiveList.ALSize].LogicalDestination = instrs.instructions[currentPc].dest;
        ActiveList.ALarray[ActiveList.ALSize].OldDestination = RegMapTable[instrs.instructions[currentPc].dest];
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
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpARegTag = -1;
        }
        else
        {
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAIsReady = true;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAValue = PhysRegFile[RegMapTable[instrs.instructions[i].src1]];
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpARegTag = -1;
        }

        // B operand is always ready for addi
        if (strcmp(instrs.instructions[i].opcode, "addi") == 0)
        {
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = true;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue = instrs.instructions[i].src2;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpBRegTag = -1;
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
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBRegTag = -1;
            }
            else
            {
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = true;
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue = PhysRegFile[RegMapTable[instrs.instructions[i].src2]];
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBRegTag = -1;
            }
        }

        printf("newReg: %d\n", newReg);

        RegMapTable[instrs.instructions[i].dest] = newReg;
        BusyBitTable[newReg] = true;
        IntegerQueue.IQSize += 1;
    }
}

/*
    Find the index of the instruction with PC in the Active List
*/
int findActiveIndex(int PC)
{
    for (size_t i = 0; i < ActiveList.ALSize; i++)
    {
        if (ActiveList.ALarray[i].PC == PC)
            return i;
    }
    return -1;
}
// TODO: change how ALU works as they are passed by reference

/*
    Pop the first element of the Active List
*/
ActiveListEntry popAL()
{ // TODO LinkedList for complexity O(1)

    ActiveListEntry temp = ActiveList.ALarray[0];

    for (size_t i = 0; i < ActiveList.ALSize - 1; i++)
    {
        ActiveList.ALarray[i] = ActiveList.ALarray[i + 1];
    }
    ActiveList.ALSize -= 1;
    return temp;
}

/*
    Pop the last element of the Active List
*/
ActiveListEntry popALBack()
{ // TODO LinkedList for complexity O(1)
    ActiveListEntry temp = ActiveList.ALarray[ActiveList.ALSize - 1];
    ActiveList.ALSize -= 1;
    return temp;
}

/*
    Issue max 4 ready instructions to the ALU
*/
void Issue()
{
    for (size_t i = 0; i < IntegerQueue.IQSize; i++)
    {
        // putting every forwardable reg into corresponding integerquueentry
        int forwardIndexA = forwardable(IntegerQueue.IQarray[i].OpARegTag);
        if (forwardIndexA >= 0)
        {
            IntegerQueue.IQarray[i].OpAIsReady = true;
            IntegerQueue.IQarray[i].OpAValue = forwardingTable.table[forwardIndexA].value;
        }
        int forwardIndexB = forwardable(IntegerQueue.IQarray[i].OpBRegTag);
        if (forwardIndexB >= 0)
        {
            IntegerQueue.IQarray[i].OpBIsReady = true;
            IntegerQueue.IQarray[i].OpBValue = forwardingTable.table[forwardIndexB].value;
        }
    }
    // scan for the 4 available entries in the Integer Queue
    int index = min(IntegerQueue.IQSize, INSTR);
    if (index <= 0)
        return;

    // ALU broadcast on 2nd cycle  TODO
    for (size_t i = 0; i < index; i++)
    {
        ALU1[i].instr = copyIQE(popReadyIQE()); // pop the oldest instruction that has both operands ready
        // show the pointer of ALU1[i].instr and ALU2[i].instr
    }
}

/*
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
    }*/

void Execute()
{
    int temp;
    for (int i = 0; i < INSTR; i++)
    {
        // printf("ALU2[%d].instr.OpCode: %s\n", i, ALU2[i].instr.OpCode);
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

        int physDestReg = ALU2[i].instr.DestRegister;
        // update the forwarding table
        forwardingTable.table[i].reg = physDestReg;
        forwardingTable.table[i].value = temp;

        // if DestRegister is not -1 then we need to update the value of the physical register and set the busy bit to false
        // printf("ALU2[%d].instr.DestRegister: %d\n", i, (ALU2[i]).instr.DestRegister);
        if (physDestReg >= 0)
        {
            // printf("ALU2[%d].instr.DestRegister: %d\n", i, (ALU2[i]).instr.DestRegister);
            // printf("temp: %d\n", temp);
            // printf("PhysRegFile[(ALU2[i]).instr.DestRegister]: %lu\n", PhysRegFile[(ALU2[i]).instr.DestRegister]);
            PhysRegFile[physDestReg] = temp;
            // printf("PhysRegFile[(ALU2[i]).instr.DestRegister]: %lu\n", PhysRegFile[(ALU2[i]).instr.DestRegister]);
            BusyBitTable[physDestReg] = false;
        }

        // printf("ALU2[%d]: %d <- ALU1[%d]: %d \n", i, (ALU2[i]).instr.DestRegister,i, (ALU1[i]).instr.DestRegister);

        // Pass the instruction to the next stage
        ALU2[i].instr = ALU1[i].instr;

        // print the pointer of ALU2[i].instr and ALU1[i].instr
        // printf("ALU2[%d]: %p <- ALU1[%d]: %p \n", i, &ALU2[i].instr, i, &ALU1[i].instr);
        // printf("ALU2[%d]: %d\n",i, (ALU2[i]).instr.DestRegister);
    }
}

/*
    Commit the instructions
*/
void Commit()
{
    // TODO "until 4 are pciked" => ?
    for (size_t i = 0; i < INSTR; i++)
    { // MAX 4 instructions

        if (ALU2[i].instr.DestRegister >= 0)
        {                                                  // if DestRegister is available (i.e. >=0)
            int index = findActiveIndex(ALU2[i].instr.PC); // find the index of the instruction with PC in the Active List
            printf("index: %d\n", index);
            if (index >= 0)
            {                                          // if the instruction is in the Active List
                ActiveList.ALarray[index].Done = true; // set the Done flag to true
            }
        }

        for (size_t i = 0; i < ActiveList.ALSize; i++)
        {
            if (ActiveList.ALarray[0].Done)
            { // if the first instruction is done, we need to remove it from the Active List

                if (ActiveList.ALarray[0].Exception)
                {
                    ePC = ActiveList.ALarray[0].PC;
                    exception = true;
                    Exception();
                    break; // TODO
                }
                else
                {
                    printf("we are entering new territory\n");
                    int archReg = ActiveList.ALarray[0].LogicalDestination;
                    printf("archReg: %d\n", archReg);
                    int physReg = RegMapTable[archReg];

                    BusyBitTable[physReg] = false; // value is not generated from the Execution stage anymore
                    printf("physReg: %d\n", physReg);

                    if (pushFreeList(physReg) == -1)
                        break; // if the Free List is full, do nothing   TODO
                    popAL();
                }
            }
            else
                break; // if DestRegister is NOT available, do nothing
            // TODO "an instruction is met that is not completed yet"
        }
    }

    // for (size_t i = 0; i < INSTR; i++) // MAX 4 instructions
    // {
    //     if (ALU2[i].instr.DestRegister >= 0) // if DestRegister is available (i.e. >=0)
    //     {
    //         //printf("ALU2[%d].instr.DestRegister: %d\n", i, ALU2[i].instr.DestRegister);
    //         int index = findActiveIndex(ALU2[i].instr.PC); // find the index of the instruction with PC in the Active List
    //         //printf("index: %d\n", index);
    //         if (index >= 0) ActiveList.ALarray[index].Done = true;
    //     }
    // }
    // //showActiveList();

    // for (size_t i = 0; i < INSTR; i++) {
    //     // if the instruction is done, we need to remove it from the Active List

    //     if (ActiveList.ALarray[0].Done) { // TODO why 0? not i?

    //         if (ActiveList.ALarray[0].Exception) {
    //             ePC = ActiveList.ALarray[0].PC;
    //             exception = true;
    //             Exception();
    //         } else {
    //             int reg = ActiveList.ALarray[0].LogicalDestination;
    //             PushFreeList(reg);
    //             BusyBitTable[reg] = false;
    //             popAL();
    //         }
    //     }
    //     else break;
    // }
}

void Exception()
{
    // reset the Integer Queue
    for (size_t i = 0; i < IntegerQueue.IQSize; i++)
    {
        IntegerQueue.IQarray[i] = createNop();
    }
    IntegerQueue.IQSize = 0;

    // reset the Execute stage
    for (size_t i = 0; i < INSTR; i++)
    {
        ALU1[i].instr = createNop();
        ALU2[i].instr = createNop();
    }

    // reset the Active List
    while (ActiveList.ALSize > 0)
    { // TODO
        // pick up 4 instructions from the Active List in reverse PC order
        for (size_t i = 0; i < INSTR; i++)
        {
            ActiveListEntry temp = popALBack(); // remove the last instruction from the Active List

            int archReg = temp.LogicalDestination;
            int physReg = RegMapTable[archReg];
            // recover the Register Map Table
            RegMapTable[archReg] = temp.OldDestination;

            BusyBitTable[physReg] = false; // set the Busy Bit to false
            // put the physical register back to the Free List
            if (pushFreeList(physReg) == -1)
                break; // if the Free List is full, do nothing   TODO
        }
    }
}

//---------------------------------------------
// Data Structure Print Functions

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
        printf("log:%d old:%d PC:%d done:%d\n", ActiveList.ALarray[i].LogicalDestination, ActiveList.ALarray[i].OldDestination, ActiveList.ALarray[i].PC, ActiveList.ALarray[i].Done);
    }
}

void showIntegerQueue()
{
    printf("IntegerQueue\n");
    for (size_t i = 0; i < IntegerQueue.IQSize; i++)
    {
        printf("dest->%d tag1:%d gtg:%d val:%d tag2:%d gtg:%d val:%d %s PC:%d\n", IntegerQueue.IQarray[i].DestRegister, IntegerQueue.IQarray[i].OpARegTag, IntegerQueue.IQarray[i].OpAIsReady, IntegerQueue.IQarray[i].OpAValue, IntegerQueue.IQarray[i].OpBRegTag, IntegerQueue.IQarray[i].OpBIsReady, IntegerQueue.IQarray[i].OpBValue, IntegerQueue.IQarray[i].OpCode, IntegerQueue.IQarray[i].PC);
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
    for (size_t i = 0; i < REGS; i++)
    {
        printf("%d ", FreeList[i]);
    }
    printf("\n");
}

void showForwardingTable()
{
    printf("ForwardingTable\n");
    for (size_t i = 0; i < INSTR; i++)
    {
        printf("reg:%d val:%d\n", forwardingTable.table[i].reg, forwardingTable.table[i].value);
    }
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

void showALU()
{
    printf("ALU1 -> ALU2\n");
    for (size_t i = 0; i < INSTR; i++)
    {
        printf("|dest->%d tag1:%d gtg:%d val:%d tag2:%d gtg:%d val:%d opcode:%s PC:%d|->|dest->%d tag1:%d gtg:%d val:%d tag2:%d gtg:%d val:%d %s PC:%d|\n",
               ALU1[i].instr.DestRegister,
               ALU1[i].instr.OpARegTag,
               ALU1[i].instr.OpAIsReady,
               ALU1[i].instr.OpAValue,
               ALU1[i].instr.OpBRegTag,
               ALU1[i].instr.OpBIsReady,
               ALU1[i].instr.OpBValue,
               ALU1[i].instr.OpCode,
               ALU1[i].instr.PC,
               ALU2[i].instr.DestRegister,
               ALU2[i].instr.OpARegTag,
               ALU2[i].instr.OpAIsReady,
               ALU2[i].instr.OpAValue,
               ALU2[i].instr.OpBRegTag,
               ALU2[i].instr.OpBIsReady,
               ALU2[i].instr.OpBValue,
               ALU2[i].instr.OpCode,
               ALU2[i].instr.PC);
    }
}

void Init()
{
    initALU();
    initForwardingTable();
    initFreeList();
}