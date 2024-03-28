#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "../lib/cJSON.h"
#include "../lib/cJSON_Utils.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "../lib/OoO470.h"

#define min(x, y) ((x) < (y) ? (x) : (y))

// Global Variables

bool finished = false;
/*
    Check if there are no more instructions to fetch
*/
int noInstruction() {
    return finished; // set by the Commit stage
}

// Instruction Buffer
Instruction instrs = {NULL, 0};

// Program Counter unsigned integer pointing to the next instruction to fetch.
unsigned int PC = 0;

// Active List
struct {
    ActiveListEntry ALarray[ENTRY];
    int ALSize;
} ActiveList;

/*
    Check if the Active List is empty
*/
int activeListIsEmpty() {
    return ActiveList.ALSize == 0;
}

/*
    Find the index of the instruction with PC in the Active List
*/
int findActiveIndex(int PC) {
    for (size_t i = 0; i < ActiveList.ALSize; i++) {
        if (ActiveList.ALarray[i].PC == PC) return i;
    }
    return -1;
}

/*
    Pop the first element of the Active List
*/
ActiveListEntry popAL() { 
    ActiveListEntry temp = ActiveList.ALarray[0];

    for (size_t i = 0; i < ActiveList.ALSize - 1; i++) {
        ActiveList.ALarray[i] = ActiveList.ALarray[i + 1];
    }
    ActiveList.ALSize -= 1;
    return temp;
}

/*
    Pop the last element of the Active List
*/
ActiveListEntry popALBack() { 
    ActiveListEntry temp = ActiveList.ALarray[ActiveList.ALSize - 1];
    ActiveList.ALSize -= 1;
    return temp;
}

// Decoded Instruction Register 
struct {
    unsigned int *DIRarray; // array that buffers instructions that have been decoded but have not been renamed and dispatched yet
    unsigned int DIRSize;   // size of the DIR
} DIR;

/*
    Pop the first element of the DIR
*/
int popDIR() {
    int temp = DIR.DIRarray[0]; 
    if (DIR.DIRSize == 0)
        return -1; // if the DIR is empty, return -1

    for (size_t i = 0; i < DIR.DIRSize - 1; i++) {
        DIR.DIRarray[i] = DIR.DIRarray[i + 1];
    }

    if (DIR.DIRSize > 1) {
        DIR.DIRarray = realloc(DIR.DIRarray, (DIR.DIRSize - 1) * sizeof(unsigned int));
        if (DIR.DIRarray == NULL)
            return -1; // if realloc fails, return -1
    } else {
        free(DIR.DIRarray);
        DIR.DIRarray = NULL;
    }

    DIR.DIRSize -= 1;
    return temp;
}

/*
    Push an instruction to the DIR
*/
int pushDIR(unsigned int instr) {
    DIR.DIRarray = realloc(DIR.DIRarray, (DIR.DIRSize + 1) * sizeof(unsigned int));
    if (DIR.DIRarray == NULL) return -1; // if realloc fails, return -1

    DIR.DIRarray[DIR.DIRSize] = instr;
    DIR.DIRSize += 1;
    return 0;
}

// Physical Register File
unsigned long PhysRegFile[REGS]; // 64 registers

FreeList freeList;

// Integer Queue
// always 32 entries myx but can be less, need to check if it is full
struct {
    IntegerQueueEntry IQarray[ENTRY];
    int IQSize;
} IntegerQueue;

/*
    Initialize the Integer Queue
*/
void initIQ() {
    IntegerQueue.IQSize = 0;
}

/*
    Create a NOP instruction in the Integer Queue
*/
IntegerQueueEntry createNop() {
    IntegerQueueEntry temp = {-1, true, -1, 0, true, -1, 0, "nop", -1};
    return temp;
}

/*
    Copy an Integer Queue Entry
*/
IntegerQueueEntry copyIQE(IntegerQueueEntry entry) {
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
    for (size_t i = 0; i < IntegerQueue.IQSize; i++) {

        if (IntegerQueue.IQarray[i].OpAIsReady && IntegerQueue.IQarray[i].OpBIsReady) {
            IntegerQueueEntry temp = IntegerQueue.IQarray[i];

            for (size_t j = i; j < IntegerQueue.IQSize - 1; j++) {
                IntegerQueue.IQarray[j] = IntegerQueue.IQarray[j + 1];
            }
            IntegerQueue.IQSize -= 1;
            return temp;
        }
    }
    IntegerQueueEntry temp = createNop();
    return temp;
}

// ALUs (E1, E2)
ALUEntry ALU1[INSTR];
ALUEntry ALU2[INSTR]; // max 4 instructions each

/*
    Initialize the ALUs
*/
void initALU() {
    for (size_t i = 0; i < INSTR; i++) {
        ALU1[i].instr = createNop();
        ALU2[i].instr = createNop();
    }
}

// Exception Flag
bool exception = false;

/*
    Get the exception flag
*/
bool getException() {
    return exception;
}

// Exception PC
unsigned int ePC = 0;

// Back Pressure Flag
bool backPressureRDS = false;

// Register Map Table
unsigned int RegMapTable[ENTRY] = { // On initialization, all architectural registers are mapped to physical registers with the same id
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31}; 
    // array that maps architectural register names to physical register names
    // 32 architectural registers, 64 physical registers

/*
    Initialize the Free List
*/
void initFreeList() { // on initialization 32-63 are free

    freeList.head = NULL;
    freeList.tail = NULL;

    // Create nodes for each register and link them together
    for (int i = (REGS - 1); i >= 32; i--) {
        FreeListNode *newNode = (FreeListNode *)malloc(sizeof(FreeListNode));
        if (i == (REGS - 1)) freeList.tail = newNode; // Set the last node as the tail
        
        newNode->value = i;
        newNode->prev = NULL;
        newNode->next = freeList.head;
        if (freeList.head != NULL) (freeList.head)->prev = newNode; // Link the previous head to the new node
        
        freeList.head = newNode; // Set the new node as the head
    }
}

/*
    Pop the first element of the Free List
*/
int popFreeList() {
    if (freeList.head == NULL) return -1; // If the list is empty, return -1
    
    int value = freeList.head->value;    // Get the value of the head
    FreeListNode *temp = freeList.head;  // Store the head in a temporary variable
    freeList.head = freeList.head->next; // Move the head to the next node

    if (freeList.head != NULL) {
        freeList.head->prev = NULL; // Set the previous node of the new head to NULL
    } else {
        freeList.tail = NULL; // If the list is empty, set the tail to NULL
    }
    free(temp);   // Free the memory of the old head
    return value; // Return the value of the old head
}

/*
    Push an element to the Free List
*/
int pushFreeList(int value) {
    FreeListNode *newNode = (FreeListNode *)malloc(sizeof(FreeListNode)); // Create a new node
    if (newNode == NULL) return -1; // If memory allocation fails, return
    
    newNode->value = value; // Set the value of the new node
    newNode->next = NULL;   // Set the next node to NULL

    if (freeList.tail != NULL) {
        freeList.tail->next = newNode; // Link the old tail to the new node
        newNode->prev = freeList.tail; // Link the new node to the old tail
    } else {
        freeList.head = newNode; // If the list is empty, set the new node as the head
        newNode->prev = NULL;    // Set the previous node to NULL
    }
    freeList.tail = newNode; // Set the new node as the tail
    return 0;
}

// Busy Bit Table
bool BusyBitTable[REGS] = {false}; // whether the value of a specific physical register will be generated from the Execution stage

/*
    Check if the register is busy
*/
bool isOpBusy(int reg) {
    return BusyBitTable[reg];
}

// Forwarding Table
typedef struct {
    forwardingTableEntry table[INSTR];
    int size;
} ForwardingTable;

ForwardingTable forwardingTable;
ForwardingTable usedForwardingTable;

/*
    Initialize the Forwarding Table
*/
void initForwardingTable() {
    for (size_t i = 0; i < INSTR; i++) {
        forwardingTable.table[i].reg = -1;
        forwardingTable.table[i].value = 0;
        forwardingTable.table[i].exception = false;

        usedForwardingTable.table[i].reg = -1;
        usedForwardingTable.table[i].value = 0;
        usedForwardingTable.table[i].exception = false;
    }
}

/*
    Check if the register is in the forwarding table
    If it is, return the index
    If it is not, return -1
*/
int forwardable(int reg) {
    for (int i = 0; i < INSTR; i++) { // 4 instructions MAX in the forwarding table because 4 ALUs
        if (forwardingTable.table[i].reg == reg && !forwardingTable.table[i].exception) return i; // if the register is in the forwarding table, return the index
    }
    return -1;
}


//---------------------------------------------
// Stages of the Pipeline

/*
    Fetch and Decode
    MAX 4 instructions per cycle 
*/
void FetchAndDecode() {

    if (exception || finished || PC == 0x10000) return;
    
    // Check Back Pressure
    if (backPressureRDS) return;

    int index = min(instrs.size - PC, INSTR); // get the first 4 instructions
    if (index <= 0) return;

    for (size_t i = 0; i < index; i++) {
        // Buffer the decoded instructions in the DIR
        pushDIR(PC);
        PC += 1;
    }
}

/*
    Rename and Dispatch
    1. Rename the instructions in the DIR
    2. Dispatch the instructions to the Active List and Integer Queue
*/
void RDS() {
    if (ActiveList.ALSize == ENTRY || IntegerQueue.IQSize == ENTRY) {
        backPressureRDS = true;
        return;
    } else backPressureRDS = false;

    if (DIR.DIRSize == 0) return;

    // Rename either all instructions in DIR or none ?     TODO
    unsigned int index = min(DIR.DIRSize, INSTR);

    for (int i = 0; i < index; i++) {
        int newReg = popFreeList();
        if (newReg < 0) return; // if the Free List is empty, do nothing
        
        unsigned int currentPc = popDIR(); // DIR can't be popped when empty because index depends on DIR size
        // if DIR size is 0, index is 0, so this loop will not run

        // Add the instruction to the Active List
        ActiveList.ALarray[ActiveList.ALSize].LogicalDestination = instrs.instructions[currentPc].dest;
        ActiveList.ALarray[ActiveList.ALSize].OldDestination = RegMapTable[instrs.instructions[currentPc].dest];
        ActiveList.ALarray[ActiveList.ALSize].PC = currentPc;
        ActiveList.ALarray[ActiveList.ALSize].Done = false;
        ActiveList.ALSize += 1;

        // Add the instruction to the Integer Queue
        IntegerQueue.IQarray[IntegerQueue.IQSize].DestRegister = newReg;
        IntegerQueue.IQarray[IntegerQueue.IQSize].PC = currentPc;

        // Add the opcode to the Integer Queue
        char *tempOpCode = "";
        if (strncmp(instrs.instructions[currentPc].opcode, "addi", 4) == 0) { // if the opcode is "addi" then we need to change it to "add"
            tempOpCode = "add";
        } else {
            tempOpCode = instrs.instructions[currentPc].opcode;
        }
        strcpy(IntegerQueue.IQarray[IntegerQueue.IQSize].OpCode, tempOpCode);

        // Check if the source registers are ready
        // if ready, forward the value to the Integer Queue
        int forwardIndexA = forwardable(RegMapTable[instrs.instructions[currentPc].src1]);

        if (forwardIndexA >= 0) {
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAIsReady = true;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAValue = forwardingTable.table[forwardIndexA].value;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpARegTag = -1;
        } else if (isOpBusy(RegMapTable[instrs.instructions[currentPc].src1])) { // if src1 the register is busy
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAIsReady = false;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpARegTag = RegMapTable[instrs.instructions[currentPc].src1];
        } else { // if src1 is ready in the physical register file
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAIsReady = true;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpAValue = PhysRegFile[RegMapTable[instrs.instructions[currentPc].src1]];
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpARegTag = -1;
        }

        // B operand is always ready for addi
        if (strncmp(instrs.instructions[currentPc].opcode, "addi", 4) == 0) {
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = true;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue = instrs.instructions[currentPc].src2;
            IntegerQueue.IQarray[IntegerQueue.IQSize].OpBRegTag = -1;
        } else {
            int forwardIndexB = forwardable(RegMapTable[instrs.instructions[currentPc].src2]);

            if (forwardIndexB >= 0) {
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = true;
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue = forwardingTable.table[forwardIndexB].value;
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBRegTag = -1;
            } else if (isOpBusy(RegMapTable[instrs.instructions[currentPc].src2])) {
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = false;
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBRegTag = RegMapTable[instrs.instructions[currentPc].src2];
            } else {
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBIsReady = true;
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBValue = PhysRegFile[RegMapTable[instrs.instructions[currentPc].src2]];
                IntegerQueue.IQarray[IntegerQueue.IQSize].OpBRegTag = -1;
            }
        }

        // Update the Register Map Table, Physical Register File, and Busy Bit Table
        RegMapTable[instrs.instructions[currentPc].dest] = newReg;
        PhysRegFile[newReg] = 0; // set the value of the physical register to 0  
        BusyBitTable[newReg] = true;

        IntegerQueue.IQSize += 1; 
    }
}

/*
    Issue Stage:
    1. Check for forwarding (ALU2 -> IntegerQueue)
    2. Issue max 4 ready instructions to the ALU1
*/
void Issue() {    

    // Put every forwardable reg into corresponding IntegerQueueEntry
    for (size_t i = 0; i < IntegerQueue.IQSize; i++) {

        int forwardIndexA = forwardable(IntegerQueue.IQarray[i].OpARegTag);
        if (forwardIndexA >= 0 && !IntegerQueue.IQarray[i].OpAIsReady) { // if the register is in the forwarding table
            IntegerQueue.IQarray[i].OpAIsReady = true;
            IntegerQueue.IQarray[i].OpAValue = forwardingTable.table[forwardIndexA].value;
        }

        int forwardIndexB = forwardable(IntegerQueue.IQarray[i].OpBRegTag);
        if (forwardIndexB >= 0 && !IntegerQueue.IQarray[i].OpBIsReady) { // if the register is in the forwarding table
            IntegerQueue.IQarray[i].OpBIsReady = true;
            IntegerQueue.IQarray[i].OpBValue = forwardingTable.table[forwardIndexB].value;
        }
    }
    // scan for the 4 available entries in the Integer Queue
    int index = min(IntegerQueue.IQSize, INSTR);
    if (index <= 0) return; // if there are no instructions in the Integer Queue, do nothing

    // ALU broadcast on 2nd cycle  
    for (size_t i = 0; i < index; i++) {
        ALU1[i].instr = copyIQE(popReadyIQE()); // pop the oldest instruction that has both operands ready
    }
}

/*
    Execute Stage:
    1. Forward the results of ALU1 to the ALU2
    2. Execute the instructions in the ALU2
*/
void Execute() {
    // Pass the instruction to the next stage
    int temp;
    for (int i = 0; i < INSTR; i++) {
        ALU2[i].instr = ALU1[i].instr;

        if (strcmp((ALU2[i]).instr.OpCode, "addi") == 0) {
            temp = (ALU2[i]).instr.OpAValue + (ALU2[i]).instr.OpBValue;
        }
        else if (strcmp((ALU2[i]).instr.OpCode, "add") == 0) {
            temp = (ALU2[i]).instr.OpAValue + (ALU2[i]).instr.OpBValue;
        }
        else if (strcmp((ALU2[i]).instr.OpCode, "sub") == 0) {
            temp = (ALU2[i]).instr.OpAValue - (ALU2[i]).instr.OpBValue;
        }
        else if (strcmp((ALU2[i]).instr.OpCode, "mulu") == 0) {
            temp = (ALU2[i]).instr.OpAValue * (ALU2[i]).instr.OpBValue;
        }
        else if (strcmp((ALU2[i]).instr.OpCode, "divu") == 0) {
            if ((ALU2[i]).instr.OpBValue == 0) { // if the divisor is 0 => exception
                usedForwardingTable.table[i].exception = true;
                usedForwardingTable.table[i].value = 0;
                usedForwardingTable.table[i].reg = (ALU2[i]).instr.DestRegister; // log which register has the exception
            } else {
                temp = (ALU2[i]).instr.OpAValue / (ALU2[i]).instr.OpBValue;
            }
        }
        else if (strcmp((ALU2[i]).instr.OpCode, "remu") == 0) {
            if ((ALU2[i]).instr.OpBValue == 0) { // if the divisor is 0 => exception
                usedForwardingTable.table[i].exception = true;
                usedForwardingTable.table[i].value = 0;
                usedForwardingTable.table[i].reg = (ALU2[i]).instr.DestRegister; 
            } else {
                temp = (ALU2[i]).instr.OpAValue % (ALU2[i]).instr.OpBValue;
            }
        } else temp = 0;

        int physDestReg = ALU2[i].instr.DestRegister;

        // Update the forwarding table
        usedForwardingTable.table[i].reg = physDestReg;
        usedForwardingTable.table[i].value = temp;
    }
}

/*
    Commit Stage: 
    1. Shift the forwarding table
    2. Commit all values from the forwarding table
    3. Remove the instructions from the Active List
    4. Remove the instructions from the ALU2
    5. Check for exceptions
*/
void Commit() {
    // Shift the forwarding table by 1 
    for (size_t i = 0; i < INSTR; i++) {
        forwardingTable.table[i] = usedForwardingTable.table[i];
    }

    // Commit 4 instructions from the forwarding table
    for(int i = 0; i < INSTR; i++) {
        int tempReg = forwardingTable.table[i].reg;
        if (tempReg >= 0) { // not a NOP
            PhysRegFile[tempReg] = (unsigned long) forwardingTable.table[i].value; // commit the value to the physical register file
            if (!forwardingTable.table[i].exception) BusyBitTable[tempReg] = false; // set the Busy Bit to false i.e. finished executing
        }  
    }

    // If the Active List is empty => exception already treated 
    // Reset the exception flag to false
    int maxIndex = min(INSTR, ActiveList.ALSize);
    if (maxIndex <= 0) {
        exception = false;
        return;
    }
    
    for (size_t i = 0; i < maxIndex; i++) {
        if (ActiveList.ALarray[0].Done) { // if the first instruction is done, we need to remove it from the Active List

            if (ActiveList.ALarray[0].Exception){
                ePC = ActiveList.ALarray[0].PC;
                exception = true;
                Exception(); // if the instruction has an exception, handle it
                break; 
            } else { // remove the instruction from the Active List
                int archReg = ActiveList.ALarray[0].LogicalDestination;
                int physReg = ActiveList.ALarray[0].OldDestination;
                                
                BusyBitTable[physReg] = false; // value is not generated from the Execution stage anymore
                
                if (pushFreeList(physReg) == -1) break; // if the Free List is full, do nothing   
                popAL();
            }
        }
        else break; // if DestRegister is NOT available, do nothing
    }

    // Set the Done flag to true for the instructions in the Active List
    for (size_t i = 0; i < INSTR; i++) { 

        if (ALU2[i].instr.DestRegister >= 0) {                                                 
            int index = findActiveIndex(ALU2[i].instr.PC); // find the index of the instruction with PC in the Active List
            if (index >= 0) {                                          
                ActiveList.ALarray[index].Done = true; // set the Done flag to true
                ActiveList.ALarray[index].Exception = forwardingTable.table[i].exception; // set the Exception flag
            }
        }
    }

    if (PC == instrs.size) finished = true;
}

// if the PC is equal to the size of the instructions, there are no more instructions to fetch
struct {
    bool state;
} delay;

/*
    Exception Handling:
    0. Set the Program Counter to 0x10000
    1. Reset the Integer Queue
    2. Reset the Execute stage
    3. Reset the DIR
    4. Reset the Active List
*/
void Exception() {
    PC = 0x10000;
    // reset the Integer Queue
    for (size_t i = 0; i < IntegerQueue.IQSize; i++) {
        IntegerQueue.IQarray[i] = createNop();
    }
    IntegerQueue.IQSize = 0;

    // reset the Execute stage
    initALU();
    initForwardingTable();

    // reset the DIR
    for (size_t i = 0; i < DIR.DIRSize; i++) {
        popDIR();
    }
    DIR.DIRSize = 0;

    if (ActiveList.ALSize == 0) return;

    // reset the Active List
    // pick up 4 instructions from the Active List in reverse PC order
    if (delay.state) {
        for (size_t i = 0; i < INSTR; i++) {
            if (ActiveList.ALSize == 0) break;
                
            ActiveListEntry temp = popALBack(); // remove the last instruction from the Active List

            int archReg = temp.LogicalDestination;
            int physReg = RegMapTable[archReg];
            // recover the Register Map Table
            RegMapTable[archReg] = temp.OldDestination;

            BusyBitTable[physReg] = false; // set the Busy Bit to false
            // put the physical register back to the Free List
            if (pushFreeList(physReg) == -1)
                break; // if the Free List is full, do nothing   
        }
    }
    // set the delay flag to true
    delay.state = true;  // TODO never set to false
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
        if (BusyBitTable[i])
        {
            printf("%d ", i);
        }
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
    printf("FreeList: \n");
    printf("freelist: %p\n", freeList);
    // go through the Free List and print the registers
    FreeListNode *current = freeList.head;
    printf("FreeList: ");
    printf("head: %p tail: %p\n", freeList.head, freeList.tail);
    while (current != NULL)
    {
        printf("%d ", current->value);
        current = current->next;
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

/*
    Initialize the system
*/
void init()
{
    initIQ();
    initFreeList();
    initALU();
    initForwardingTable();
}

//---------------------------------------------
// JSON

/*
    Parser for JSON file
*/
int parser(char *file_name) {
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
            printf("Error before: %s\n", error_ptr);
        }
        cJSON_free(json_data); // Free memory
        return 1;
    }

    // Iterate over the array and print each string
    cJSON *instruction;

    size_t array_size = cJSON_GetArraySize(root);

    if (array_size == 0)
    {
        printf("Error: Empty or invalid JSON array.\n");
        cJSON_Delete(root);
        free(json_data);
        return 1;
    }

    instrs.instructions = (InstructionEntry *)malloc(array_size * sizeof(InstructionEntry));
    if (instrs.instructions == NULL)
    {
        printf("Instruction memory allocation failed.\n");
        cJSON_Delete(root);
        free(json_data);
        return 1;
    }
    instrs.size = 0;

    cJSON_ArrayForEach(instruction, root)
    {
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
                // remove the 'x' from the string
                token = token + 1; 
                sscanf(token, "%d", &instrs.instructions[instrs.size].dest);

                token = strtok(NULL, ",");
                if (token != NULL)
                {
                    // remove the 'x' from the string
                    token = token + 2;
                    sscanf(token, "%d", &instrs.instructions[instrs.size].src1);
                    token = strtok(NULL, "\0"); 
                    if (token != NULL)
                    { // check if src2 starts by 'x' if so remove it
                        if (token[1] == 'x')
                        {
                            token = token + 2; 
                            sscanf(token, "%d", &instrs.instructions[instrs.size].src2);
                        } else {
                            sscanf(token, "%d", &instrs.instructions[instrs.size].src2);
                        }
                    }
                }
            }
        }
        instrs.size += 1;
    }

    // Free memory
    cJSON_Delete(root);
    free(json_data);

    return 0;
}

/*
    Output the system state in JSON format to a file
*/
void outputSystemStateJSON(FILE *file) {
    fprintf(file, "{\n");

    fprintf(file, "  \"ActiveList\": [\n");
    for (int i = 0; i < ActiveList.ALSize; ++i)
    {
        fprintf(file, "    {\n");
        fprintf(file, "      \"Done\": %s,\n", ActiveList.ALarray[i].Done ? "true" : "false");
        fprintf(file, "      \"Exception\": %s,\n", ActiveList.ALarray[i].Exception ? "true" : "false");
        fprintf(file, "      \"LogicalDestination\": %d,\n", ActiveList.ALarray[i].LogicalDestination);
        fprintf(file, "      \"OldDestination\": %d,\n", ActiveList.ALarray[i].OldDestination);
        fprintf(file, "      \"PC\": %d\n", ActiveList.ALarray[i].PC);
        fprintf(file, "    }%s\n", (i < ActiveList.ALSize - 1) ? "," : ""); // Add comma if not the last element
    }
    fprintf(file, "  ],\n");

    fprintf(file, "  \"BusyBitTable\": [");
    for (int i = 0; i < REGS; i++)
    {
        fprintf(file, "%s%s", (i > 0 ? ", " : ""), (BusyBitTable[i] ? "true" : "false"));
    }
    fprintf(file, "],\n");

    fprintf(file, "  \"DecodedPCs\": [\n");
    // Output DecodedPCs array
    for (unsigned int i = 0; i < DIR.DIRSize; ++i)
    {
        fprintf(file, "    %u", DIR.DIRarray[i]);
        if (i < DIR.DIRSize - 1)
        {
            fprintf(file, ",");
        }
        fprintf(file, "\n");
    }
    fprintf(file, "  ],\n");

    fprintf(file, "  \"Exception\": %s,\n", (exception ? "true" : "false"));
    fprintf(file, "  \"ExceptionPC\": %u,\n", ePC);

    fprintf(file, "  \"FreeList\": [");
    FreeListNode *current = freeList.head;
    while (current != NULL)
    {
        fprintf(file, "%s%d", (current != freeList.head ? ", " : ""), current->value);
        current = current->next;
    }
    fprintf(file, "],\n");

    fprintf(file, "  \"IntegerQueue\": [\n");
    for (int i = 0; i < IntegerQueue.IQSize; ++i)
    {
        fprintf(file, "    {\n");
        fprintf(file, "      \"DestRegister\": %d,\n", IntegerQueue.IQarray[i].DestRegister);
        fprintf(file, "      \"OpAIsReady\": %s,\n", IntegerQueue.IQarray[i].OpAIsReady ? "true" : "false");
        fprintf(file, "      \"OpARegTag\": %d,\n", IntegerQueue.IQarray[i].OpARegTag);
        fprintf(file, "      \"OpAValue\": %lu,\n", IntegerQueue.IQarray[i].OpAValue);
        fprintf(file, "      \"OpBIsReady\": %s,\n", IntegerQueue.IQarray[i].OpBIsReady ? "true" : "false");
        fprintf(file, "      \"OpBRegTag\": %d,\n", IntegerQueue.IQarray[i].OpBRegTag);
        fprintf(file, "      \"OpBValue\": %lu,\n", IntegerQueue.IQarray[i].OpBValue);
        fprintf(file, "      \"OpCode\": \"%s\",\n", IntegerQueue.IQarray[i].OpCode);
        fprintf(file, "      \"PC\": %d\n", IntegerQueue.IQarray[i].PC);
        fprintf(file, "    }%s\n", (i < IntegerQueue.IQSize - 1) ? "," : ""); // Add comma if not the last element
    }
    fprintf(file, "  ],\n");

    fprintf(file, "  \"PC\": %u,\n", PC);

    fprintf(file, "  \"PhysicalRegisterFile\": [");
    for (int i = 0; i < REGS; i++)
    {
        fprintf(file, "%s%lu", (i > 0 ? ", " : ""), PhysRegFile[i]);
    }
    fprintf(file, "],\n");

    fprintf(file, "  \"RegisterMapTable\": [");
    for (int i = 0; i < ENTRY; i++)
    {
        fprintf(file, "%s%lu", (i > 0 ? ", " : ""), RegMapTable[i]);
    }
    fprintf(file, "]");

    fprintf(file, "}\n");
}

int slog(char *f_out, int i) {
    FILE *outputFile = fopen(f_out, "a"); // Open file for writing
    if (outputFile == NULL)
    {
        perror("Error opening file");
        return 1;
    }

    if (i == 0)
    { // Initial '{' in output JSON file
        fprintf(outputFile, "[\n");
    }
    else if (i == 1)
    { // Final '}' in output JSON file
        fprintf(outputFile, "]\n");
    }
    else if (i == 2)
    { // Add comma if not the first cycle and not the last element logged
        fprintf(outputFile, ",\n");
    }
    else
    {
        // Call the function to output the JSON representation to the file
        outputSystemStateJSON(outputFile);
    }

    fclose(outputFile); // Close the file
    return 0;
}

//---------------------------------------------
void propagate() {
    // 5. Commit
    Commit();
    // 4. Execute
    Execute();
    // 3. Issue
    Issue(); 
    // 2. Rename & Dispatch
    RDS(); 
    // 1. Fetch & Decode
    FetchAndDecode();
}
