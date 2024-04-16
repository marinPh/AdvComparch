#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/cJSON.h"
#include "lib/cJSON_Utils.h"
#include "VLIW470.h"

int parser(char *file_name, Instruction instrs) {

    // Open the JSON file
    FILE *file = fopen(file_name, "r");
    if (file == NULL) {
        printf("Failed to open file.\n");
        return 1;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file); // Go back to the beginning of the file

    // Allocate memory to store the file contents
    char *json_data = (char *)malloc(file_size + 1);
    if (json_data == NULL)
    {
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

    instrs.instructions = (InstructionEntry *)malloc(array_size * sizeof(InstructionEntry));
    if (instrs.instructions == NULL) {
        printf("Instruction memory allocation failed.\n");
        cJSON_Delete(root);
        free(json_data);
        return 1;
    }
    instrs.size = 0;

    cJSON_ArrayForEach(instruction, root) {

        // Parse at whitespace and eliminate ',' and store in InstructionEntry

        // Copy the instruction string since strtok modifies the input
        char *instruction_copy = strdup(instruction->valuestring);

        // Tokenize the instruction string
        char *token = strtok(instruction_copy, " ");

        if (token != NULL) {

            strcpy(instrs.instructions[instrs.size].opcode, token); // already null-terminated

            // case NOP
            if (strcmp(instrs.instructions[instrs.size].opcode, "nop") == 0) {
                instrs.instructions[instrs.size].dest = -1; 
                instrs.instructions[instrs.size].src1 = -1;
                instrs.instructions[instrs.size].src2 = -1; 

                instrs.size += 1;
                continue;  // TODO does this go to next elem in JSON array ?
            }

            token = strtok(NULL, ",");
            if (token != NULL) {
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
                    if (token != NULL) { // check if src2 starts by 'x' if so remove it
                        if (token[0] == 'x')
                        {
                            token = token + 2;
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
        instrs.size += 1;
    }

    // Free memory
    cJSON_Delete(root);
    free(json_data);

    return 0;
}