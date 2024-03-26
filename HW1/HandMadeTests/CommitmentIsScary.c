#include <stdio.h>
#include "../OoO470.h"

int main(int argc, char *argv[]) {
    printf("Hello, World!\n");
  parser("../given_tests/01/input.json");

  init();
  printf("%s", "init\n");
  

  
  for (int i = 0; i < 10; i++) {
    //showForwardingTable();
    //showALU();
    //showPhysRegFile();
    //showActiveList();
    //showBusyBitTable();
    showRegMapTable();
    //showDIR();
    showFreeList();
  //showIntegerQueue();
    showActiveList();
    showALU();
    //showIntegerQueue();
    Commit();
    printf("Commit, [%d]\n", i);
    Execute();
    //showALU();
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