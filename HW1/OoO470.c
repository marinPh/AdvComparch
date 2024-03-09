#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <jansson.h>
#include <cjson/cJSON.h>

#define REGS 64
#define ENTRY 32
#define INSTR 4

// Program Counter unsigned integer pointing to the next instruction to fetch.
unsigned int PC = 0;

// Physical Register File
unsigned long PhysRegFile[REGS]; // 64 registers of 64 bits each

// Decoded Instruction Register
unsigned int * DIR; // array that buffers instructions that have been decoded but have not been renamed and dispatched yet
unsigned int DIRSize = 0; // size of the DIR

// Exception Flag
bool exception = false;

// Exception PC
unsigned int ePC = 0;

// Register Map Table
unsigned int RegMapTable[ENTRY] = {  // On initialization, all architectural registers are mapped to physical registers with the same id
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
    30, 31
}; // array that maps architectural register names to physical register names
// 32 architectural registers, 64 physical registers

// Free List
unsigned int * FreeList; // array that keeps track of the physical registers that are free
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

// Active List
// instructions that have been dispatched but have not yet completed
// renamed instructions
ActiveListEntry ActiveList[ENTRY]; 

// Entry in Integer Queue
typedef struct {
    int DestRegister;
    bool OpAIsReady;
    int OpARegTag; // for cheking forwarding 
    int OpAValue;
    bool OpBIsReady;
    int OpBRegTag;
    int OpBValue;
    char OpCode[5];  // 4 characters + null terminator
    int PC;
} IntegerQueueEntry;

// Integer Queue
// instructions awaiting issuing
IntegerQueueEntry IntegerQueue[ENTRY]; 


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
    cJSON_ArrayForEach(instruction, root) {
        printf("%s\n", instruction->valuestring);
    }

    // Free memory
    cJSON_Delete(root);
    free(json_data);

    return 0;




    // Initialize the Free List
    FreeList = (unsigned int *)malloc((REGS - 32) * sizeof(unsigned int));
    for (int i = 0; i < (REGS - 32); i++) {
        FreeList[i] = 32 + i;
    }

    // Fetch & Decode
    FetchAndDecode();

    // Rename
    Rename();

    // Dispatch
    Dispatch();

    // Issue
    Issue();

    // Execute
    Execute();

    // Write Result
    WriteResult();

    // Commit
    Commit();

    return 0;
}



// Fetch & Decode
void FetchAndDecode() {
    // Fetch MAX 4 instructions from memory

    // Decode the instructions
    // get first 4 instructions from buffer
    // read JSON file until the end 


    const char *json_file = "your_file_path.json";  // Replace with your actual file path
    json_t *root;
    json_error_t error;

    root = json_loads(buffer, 0, &error);
    if (!root) {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return 1;
    }

    size_t index = json_array_size(root);
    index = math.min(index, INSTR); // get the first 4 instructions

    for (size_t i = 0; i < index; i++) {
        json_t *instruction = json_array_get(root, i);

        json_t *opcode = json_object_get(instruction, "opcode");
        json_t *dest = json_object_get(instruction, "dest");
        json_t *src1 = json_object_get(instruction, "src1");
        json_t *src2 = json_object_get(instruction, "src2");

        IntegerQueue[i].DestRegister = json_integer_value(dest);  // TODO not i but the actual index
        IntegerQueue[i].OpARegTag = json_integer_value(src1);
        IntegerQueue[i].OpBRegTag = json_integer_value(src2);
        strcpy(IntegerQueue[i].OpCode, json_string_value(opcode));
        IntegerQueue[i].PC = PC;

        // Check whether the source registers are ready
        if (BusyBitTable[IntegerQueue[i].OpARegTag] == false) { 
            IntegerQueue[i].OpAIsReady = true;
            IntegerQueue[i].OpAValue = PhysRegFile[IntegerQueue[i].OpARegTag];
        } else {
            IntegerQueue[i].OpAIsReady = false;
        }

        // Buffer the decoded instructions in the DIR
        DIR = realloc(DIR, (DIRSize + 1) * sizeof(unsigned int)); // increase the size of the DIR by 1   TODO better way for performance !!
        DIR[DIRSize] = PC; 
        DIRSize += 1;

        // Update the PC
        PC += 1; 
    }
}