//
// Created by marin on 23/04/2024.
//

#include <stdio.h>
#include <stdlib.h>

#include "lib/VLIW470.h"


int main()
{
    parseInstrunctions("../simulator/program.json", "../given_tests/01/input.json");
    return 0;
}
