#include <stdio.h>
#include "../OoO470.h"

int main(int argc, char *argv[]) {
    printf("Hello, World!\n");
  parser("../given_tests/02/input.json");
  showInstruction();
  FetchAndDecode();
  showDIR();
  

  return 0;
}