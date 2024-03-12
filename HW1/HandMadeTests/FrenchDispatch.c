#include <stdio.h>
#include "../OoO470.h"

int main(int argc, char *argv[]) {
    printf("Hello, World!\n");
  parser("../given_tests/02/input.json");
  showInstruction();
  
  for (int i = 0; i < 3; i++) {
    showDIR();
    RDS();
    showFreeList();
    FetchAndDecode();
    showIntegerQueue();
  }

  

  return 0;
}