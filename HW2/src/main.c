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
    
    DependencyTable table = createFillDepencies();
    //showDepTable(table);

    // test LOOP (simple)
    scheduleInstructions(state, &table);

    registerAllocation(state, &table);
    //showProcessorState(*state);   

    writeVLIWToJson(&state->bundles, argv[2]); 
    //printf("output.json created\n");

    // free memory
    free(state->bundles.vliw);
    free(state);

    emptyInstructions(); 

    //--------------------------------------------------------------------------------

    parseInstrunctions(argv[1]);

    // test LOOP_PIP
    ProcessorState *state_pip = (ProcessorState *)malloc(sizeof(ProcessorState));
    if (state_pip == NULL) {
        printf("Memory allocation failed.\n");
        return 1;
    }
    initProcessorState(state_pip);
    //showProcessorState(*state);

    DependencyTable table_pip = createFillDepencies();
    //showDepTable(table_pip);

    scheduler(state_pip, &table_pip);
    // registerAllocationPip(state_pip, &table_pip);

    writeVLIWToJson(&state_pip->bundles, argv[3]); 
    //printf("output.json created\n");

    // free memory
    free(state_pip->bundles.vliw);
    free(state_pip);

    return 0;
}