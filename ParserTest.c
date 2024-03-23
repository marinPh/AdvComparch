#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"

int main(
    int argc,
    char *argv[])
{
    /*
    Instruction *list;
    int ok = Parse(list);*/
    return 0;
}
/*
struct
{
    char *instruction;
    int src1;
    int src2;
    int dest;
    int imm;
} typedef Instruction;

int Parse(Instruction *list)
{

    // Open the JSON file
    FILE *file = fopen("input.json", "r");
    if (file == NULL)
    {
        printf("Failed to open the file.\n");
        return 1;
    }

    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Allocate memory to store the file contents
    char *json_data = (char *)malloc(file_size + 1);
    list = (Instruction *)malloc(file_size + 1);
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
    cJSON_ArrayForEach(instruction, root)
    {

        char *instruction_string = strdup(instruction->valuestring); // Duplicate the string
        char *token = strtok(instruction_string, " ");               // Split the string by spaces
        while (token != NULL)
        {
            // Remove commas from the token
            char *comma_ptr = strchr(token, ',');
            while (comma_ptr != NULL)
            {
                memmove(comma_ptr, comma_ptr + 1, strlen(comma_ptr));
                comma_ptr = strchr(comma_ptr, ',');
            }
            printf("%s\n okok", token);
            token = strtok(NULL, " ");
            free(instruction_string); // Free the duplicated string
        }
        // Free memory
        cJSON_Delete(root);
        free(json_data);

        return 0;
    }
}
*/