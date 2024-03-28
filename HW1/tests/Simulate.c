
#include <stdio.h>
#include "../lib/OoO470.h"

int main(int argc, char *argv[])
{
    // Check if the correct number of arguments is provided
    if (argc != 3) return 1;

    char* def_input = "../given_tests/07/input.json";
    def_input = argv[1];

    // 0. Parse the JSON file
    if (parser(def_input) != 0) {
        printf("Failed to parse the JSON file.\n");
        return 1;
    }

    char *output_file = argv[2];
    if (output_file == NULL) {
        printf("Output file not provided.\n");
        return 1;
    }

    // 1. Initialize the system
    init();

    slog(output_file, BEGINLOG); // Initial '{' in output JSON file

    // 2. Dump the state of the reset system
    slog(output_file, LOG); 
    int cycle = 0;
    // 3. Loop for cycle-by-cycle iterations
    while (!(noInstruction() && activeListIsEmpty() && !getException()))   {
        // printf("No instruction: %d\n", noInstruction());
        // printf("Active list is empty: %d\n", activeListIsEmpty());
        // printf("Cycle: %d\n", cycle++);

        //showActiveList();
        slog(output_file, LOGCOMMA); // Add comma if not the first cycle and not the last element logged
        propagate();
        slog(output_file, LOG); 
    }
    // Final '}' in output JSON file
    // 4. save the output JSON log
    slog(output_file, ENDLOG); 

    // // Free memory
    // free(instrs.instructions);

    return 0;
}