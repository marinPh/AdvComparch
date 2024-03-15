#include <stdio.h>
#include "../OoO470.h"

int main(int argc, char *argv[]) {
    printf("Hello, World!\n");
  parser("../given_tests/01/input.json");

  initALU();
  printf("%s", "initALU\n");
  initForwardingTable();
    printf("%s", "initForwardingTable\n");

  //open output file and add [
  FILE *fp = fopen("output.json", "w");
  fprintf(fp, "[\n");
  fclose(fp);
  for (int i = 0; i < 9; i++) {
    //showForwardingTable();
    toJsonTotal("output.json");
    showALU();
    showPhysRegFile();

    //showBusyBitTable();
    showIntegerQueue();
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
  fp = fopen("output.json", "a");
  //remove last comma
  fseek(fp, -3, SEEK_END);
  fprintf(fp, "]\n");
  fflush(fp);
  fclose(fp);
  return 0;
}
