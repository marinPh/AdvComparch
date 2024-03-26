
#include <stdio.h>
#include "../OoO470.h"

// int main(int argc, char *argv[]) {

//   // get from argv the path to the input file and output file

//   char *input_file = argv[1];
//   char *output_file = argv[2];
//   if (argc != 3) {
//       printf("Usage: %s <input_file> <output_file>\n", argv[0]);
//       return 1;
//   }

//   parser(input_file);

//   Init();
  

//   for (int i = 0; i < 10; i++) {
//     Propagate();
//   }
//   return 0;
// }

int main(int argc, char *argv[])
{
    // Check if the correct number of arguments is provided
    if (argc != 3)
    {
        return 1;
    }

    // 0. Parse the JSON file
    if (parser(argv[1]) != 0)
    {
        printf("Failed to parse the JSON file.\n");
        return 1;
    }

    char *output_file = argv[2];
    if (output_file == NULL)
    {
        printf("Output file not provided.\n");
        return 1;
    }

    // 1. Initialize the system
    init();

    slog(output_file, BEGINLOG); // Initial '{' in output JSON file

    // 2. Dump the state of the reset system
    slog(output_file, LOG); // TODO dumpStateIntoLog()

    // 3. Loop for cycle-by-cycle iterations
    while (!(noInstruction() && activeListIsEmpty()))  
    {
        slog(output_file, LOGCOMMA); // Add comma if not the first cycle and not the last element logged

        // do propagation
        // if you have multiple modules, propagate each of them
        propagate();
        // advance clock, start next cycle
        //latch();
        // dump the state
        slog(output_file, LOG); // TODO dumpStateIntoLog()
    }
    // Final '}' in output JSON file
    // 4. save the output JSON log
    slog(output_file, ENDLOG); // TODO saveLog() necessary ? 

    // Free memory
    //free(instrs.instructions);

    return 0;
}