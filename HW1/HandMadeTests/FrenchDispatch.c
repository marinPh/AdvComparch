#include <stdio.h>
#include "../OoO470.h"

int main(int argc, char *argv[]) {
    printf("Hello, World!\n");
  parser("../given_tests/01/input.json");
  showInstruction();
  
  for (int i = 0; i < 7; i++) {
    showDIR();
    
    RDS();
    showRegMapTable();
    showFreeList();
    FetchAndDecode();
    showIntegerQueue();
  }

  

  return 0;
}