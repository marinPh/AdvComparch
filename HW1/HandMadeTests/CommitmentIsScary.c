#include <stdio.h>
#include "../OoO470.h"

int main(int argc, char *argv[]) {
    printf("Hello, World!\n");
  parser("../given_tests/01/input.json");

  initALU();
  printf("%s", "initALU\n");
  initForwardingTable();
    printf("%s", "initForwardingTable\n");

  
  for (int i = 0; i < 7; i++) {
    //showForwardingTable();
    //showALU();
    //showPhysRegFile();
    //showActiveList();
    //showBusyBitTable();
    //showIntegerQueue();
    Commit();
    printf("Commit, [%d]\n", i);
    Execute();
    showALU();
    printf("Execute, [%d]\n", i);
    Issue();
 
    printf("Issue, [%d]\n", i);
    RDS();
    printf("RDS, [%d]\n", i);
    FetchAndDecode();
    printf("FetchAndDecode, [%d]\n", i);
  }

  

  return 0;
}