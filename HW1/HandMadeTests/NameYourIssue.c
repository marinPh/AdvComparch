#include <stdio.h>
#include "../OoO470.h"

int main(int argc, char *argv[]) {
    printf("Hello, World!\n");
  parser("../given_tests/01/input.json");
  showInstruction();
  initALU();
  printf("%s", "initALU\n");
  initForwardingTable();
    printf("%s", "initForwardingTable\n");
  showForwardingTable();
  
  for (int i = 0; i < 4; i++) {
 
    showForwardingTable();

    showALU();
    Execute();
    printf("Execute, [%d]", i);
    showALU();
    showForwardingTable();
    Issue();
    printf("Issue, [%d]", i);
    RDS();
    printf("RDS, [%d]", i);
    FetchAndDecode();
    printf("FetchAndDecode, [%d]", i);
  }

  

  return 0;
}