// main function
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/cJSON.h"
#include "../lib/cJSON_Utils.h"
#include "../lib/VLIW470.h"

int main(int argc, char *argv[]) {
    if (argc <= 3) {
        printf("Usage: %s <file_name>\n", argv[0]);
        return 1;
    }

    parseInstrunctions(argv[1]);

    
    
    ProcessorState *state = (ProcessorState *)malloc(sizeof(ProcessorState));
    initProcessorState(state);
    //showProcessorState(*state);
    
    printf("filling dep\n");
    DependencyTable table = createFillDepencies();
    //showDepTable(table);

    // test LOOP (simple)
    scheduleInstructions(state, &table);
//
    registerAllocation(state, &table);
    printf("II: \n");
    //showProcessorState(*state);   

    writeVLIWToJson(&state->bundles, argv[2]); 
    writeVLIWToJson(&state->bundles, argv[3]);
    printf("output.json created\n");


    // test LOOP_PIP
    // ProcessorState *state_pip = (ProcessorState *)malloc(sizeof(ProcessorState));
    // if (state_pip == NULL) {
    //     printf("Memory allocation failed.\n");
    //     fclose(file);
    //     return 1;
    // }
    // initProcessorState(state_pip);

    // scheduleInstructionsPiP(state_pip, &table);
    // registerAllocationPip(state_pip, &table);

    // free memory
    free(state->bundles.vliw);
    free(state);
    // free(state_pip->bundles.vliw);
    // free(state_pip);

    return 0;
}