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

// Structure for parsing JSON entry
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

// Program Counter unsigned integer pointing to the next instruction to fetch.
unsigned int PC = 0;

// Physical Register File
unsigned long PhysRegFile[REGS]; // 64 registers of 64 bits each

// Decoded Instruction Register
/*
unsigned int * DIR; // array that buffers instructions that have been decoded but have not been renamed and dispatched yet
unsigned int DIRSize = 0; // size of the DIR*/
struct{
    unsigned int * DIRarray; // array that buffers instructions that have been decoded but have not been renamed and dispatched yet
    unsigned int DIRSize; // size of the DIR
}DIR;

// Exception Flag
bool exception = false;

// Exception PC
unsigned int ePC = 0;

bool backPressureRDS = false;


// Register Map Table
unsigned int RegMapTable[ENTRY] = {  // On initialization, all architectural registers are mapped to physical registers with the same id
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
    30, 31
}; // array that maps architectural register names to physical register names
// 32 architectural registers, 64 physical registers

// Free List
unsigned int * FreeList[ENTRY] = {
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    62, 63
}; // array that keeps track of the physical registers that are free
 // array that keeps track of the physical registers that are free
// on initialization 32-63 are free

// Busy Bit Table
bool BusyBitTable[REGS] = {false}; // whether the value of a specific physical register will be generated from the Execution stage

// Entry in the Active List
typedef struct {
    bool Done;
    bool Exception;
    int LogicalDestination;
    int OldDestination;
    int PC;
} ActiveListEntry;
int PopFreeList(){
    int reg = FreeList[0];
    for (size_t i = 0; i < ENTRY - 1; i++) {
        FreeList[i+1] = FreeList[i];
    }
    FreeList[ENTRY - 1] = -1;
    return reg;
}

int getFreeReg(){

    return PopFreeList();
}



int PushFreeList(int reg){
    if(FreeList[ENTRY - 1] == -1){
        return -1;
    }
    for (size_t i = 0; i < ENTRY; i++) {
        FreeList[i] = FreeList[i+1];
    }
    FreeList[0] = reg;
    return 0;
    
}

bool forwardable(int reg){
    //TODO: implement
    return true;
}




// Active List
// instructions that have been dispatched but have not yet completed
// renamed instruction
/*
ActiveListEntry ActiveList[ENTRY]; */

struct {
    ActiveListEntry ALarray[ENTRY];
    int ALSize;
} ActiveList;



// Entry in Integer Queue
typedef struct {
    int DestRegister;
    bool OpAIsReady;
    int OpARegTag; // for cheking forwarding 
    int OpAValue;
    bool OpBIsReady;
    int OpBRegTag;
    int OpBValue;
    char OpCode[OPCODE];  // 4 characters + null terminator
    int PC;
} IntegerQueueEntry;

// Integer Queue
/*
IntegerQueueEntry IntegerQueue[ENTRY]; */
//always 32 entries myx but can be less, need to check if it is full
struct {
    IntegerQueueEntry IQarray[ENTRY];
    int IQSize;
} IntegerQueue;




int main() {

    // Open the JSON file
    FILE *file = fopen("data.json", "r");
    if (file == NULL) {
        printf("Failed to open the file.\n");
        return 1;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file); // Go back to the beginning of the file

    // Allocate memory to store the file contents
    char *json_data = (char *)malloc(file_size + 1);
    if (json_data == NULL) {
        printf("Memory allocation failed.\n");
        fclose(file);
        return 1;
    }

    // Read the file contents into the allocated memory
    fread(json_data, 1, file_size, file);
    json_data[file_size] = '\0'; // Null-terminate the string

    // Close the file
    fclose(file);

    // Parse the JSON data
    cJSON *root = cJSON_Parse(json_data);
    if (root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printf("Error before: %s\n", error_ptr);
        }
        cJSON_free(json_data); // Free memory
        return 1;
    }

    // Iterate over the array and print each string
    cJSON *instruction;

    size_t array_size = cJSON_GetArraySize(root);

    if (array_size == 0) {
        printf("Error: Empty or invalid JSON array.\n");
        cJSON_Delete(root);
        free(json_data);
        return 1;
    }

    Instruction instrs = {NULL, 0};
    instrs.instructions = (InstructionEntry *)malloc(array_size * sizeof(InstructionEntry));
    if(instrs.instructions == NULL) {
        printf("Instruction memory allocation failed.\n");
        cJSON_Delete(root);
        free(json_data);
        return 1;
    }
    instrs.size = 0;

    cJSON_ArrayForEach(instruction, root) {
        //printf("%s\n", instruction->valuestring);

        // Parse at whitespace and eliminate ',' and store in InstructionEntry

        // Copy the instruction string since strtok modifies the input
        char *instruction_copy = strdup(instruction->valuestring);

        // Tokenize the instruction string
        char *token = strtok(instruction_copy, ' ');
        
        if (token != NULL) {
            strcpy(instrs.instructions[instrs.size].opcode, token); // already null-terminated

            token = strtok(NULL, ','); 
            if (token != NULL) {
                sscanf(token, "%d", &instrs.instructions[instrs.size].dest);

                token = strtok(NULL, ',');
                if (token != NULL) {
                    sscanf(token, "%d", &instrs.instructions[instrs.size].src1);

                    token = strtok(NULL, '\0');
                    if (token != NULL) {
                        sscanf(token, "%d", &instrs.instructions[instrs.size].src2);
                    }
                }
            }
        }
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

int Commit(){
    return 0;
}

bool isOpBusy(int reg){
    return BusyBitTable[reg];
}





// Fetch & Decode
void FetchAndDecode(Instruction * instr) {
    // Fetch MAX 4 instructions from memory

    // Decode the instructions
    // get first 4 instructions from buffer
    // read JSON file until the end 

   

/* 
When the Commit stage
 indicates that an exception is detected, the PC is set to 0x10000
 */
//TODO: create Commit Stage
    if(Commit()<0)
{
    // set at 0x10000
    PC = 0x10000;
    
}
//Check Back Pressure 
if (backPressureRDS){
    return;
}
    
    int index = min(instr->size - PC, INSTR); // get the first 4 instructions
    if (index <= 0) {
        return;
    }
    for (size_t i = 0; i < index; i++) {

        // Buffer the decoded instructions in the DIR
        DIR.DIRarray = realloc(DIR.DIRarray, (DIR.DIRSize + 1) * sizeof(unsigned int)); // increase the size of the DIR by 1   TODO better way for performance !!
        DIR.DIRarray[DIR.DIRSize] = PC; 
        DIR.DIRSize += 1;
        PC += 1; 
    }
}

// Rename

void RDS (Instruction * instr){
    
    if (DIR.DIRSize == 0  || ActiveList.ALSize == ENTRY || IntegerQueue.IQSize == ENTRY){
        backPressureRDS = true;
        return;
    }else{
        backPressureRDS = false;
    }
    int newReg = getFreeReg();
    if (newReg < 0){
        return;
    }
    // Rename the instructions
    for (size_t i = 0; i < DIR.DIRSize; i++) {
        // make it busy
        BusyBitTable[newReg] = true;
        unsigned int oldReg = RegMapTable[instr[i].instructions->dest];
        // map the logical destination to the physical register
        RegMapTable[instr[i].instructions->dest] = newReg;
        // add the instruction to the Active List
        ActiveList.ALarray[ActiveList.ALSize].LogicalDestination = instr[i].instructions->dest;
        ActiveList.ALarray[ActiveList.ALSize].OldDestination = oldReg,
        ActiveList.ALarray[ActiveList.ALSize].PC = DIR.DIRarray[i];
        ActiveList.ALSize += 1;
        // add the instruction to the Integer Queue
        IntegerQueue.IQarray[IntegerQueue.IQSize].DestRegister = newReg;
        IntegerQueue.IQarray[IntegerQueue.IQSize].PC = DIR.DIRarray[i];
        const char* tempOpCode = instr[i].instructions->opcode;
        strcpy(IntegerQueue.IQarray[IntegerQueue.IQSize].OpCode, tempOpCode);
        

       // we want to check if the source registers are ready
       // if they are ready, we want to forward the value to the Integer Queue
       // if opcode is "addi" then we only need to check if src1 is ready
       // and we give the value of src2 to IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue
         // if opcode is not "addi" then we need to check if both src1 and src2 are ready
         // and we give the value of src1 to IntegerQueue.IQarray[IntegerQueue.IQSize].OpAValue
        
        if (isOpBusy(RegMapTable[instr[i].instructions->src1])){
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAIsReady = false;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpARegTag = RegMapTable[instr[i].instructions->src1];
        }else if(forwardable(RegMapTable[instr[i].instructions->src1])){
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAIsReady = true;
            //TODO: check value from forwarding table
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAValue = PhysRegFile[RegMapTable[instr[i].instructions->src1]];

        }else{
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAIsReady = true;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAValue = PhysRegFile[RegMapTable[instr[i].instructions->src1]];
        }

        if (strcmp(instr[i].instructions->opcode, "addi")==0){
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = true;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue = instr[i].instructions->src2;

        }else{

            if (isOpBusy(RegMapTable[instr[i].instructions->src2])){
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = false;
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBRegTag = RegMapTable[instr[i].instructions->src2];
            }else if(forwardable(RegMapTable[instr[i].instructions->src2])){
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = true;
                //TODO: check value from forwarding table
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue = PhysRegFile[RegMapTable[instr[i].instructions->src2]];
            }else{
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = true;
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue = PhysRegFile[RegMapTable[instr[i].instructions->src2]];
            }

           
        }
        IntegerQueue.IQSize += 1;
    }
    // Clear the DIR
    free(DIR.DIRarray);
       return;
    }

    void IEF(){
        //scan for the 4 

    }


    
    



    

    // Rename, Dispatch, and Issue instructions