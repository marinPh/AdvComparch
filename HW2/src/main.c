// main function
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/cJSON.h"
#include "lib/cJSON_Utils.h"
#include "lib/VLIW470.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <file_name>\n", argv[0]);
        return 1;
    }

    // Open the JSON file
    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        printf("Failed to open file.\n");
        return 1;
    }

    parseInstrunctions(file);

    fclose(file);

    ProcessorState *state = (ProcessorState *)malloc(sizeof(ProcessorState));
    if (state == NULL) {
        printf("Memory allocation failed.\n");
        fclose(file);
        return 1;
    }
    initProcessorState(state);

    DependencyTable table = createFillDepencies();
    showDepTable(table);

    // test LOOP (simple)
    scheduleInstructions(state, &table);
    registerAllocation(state, &table);


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