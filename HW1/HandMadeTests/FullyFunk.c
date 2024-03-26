
#include <stdio.h>
#include "../OoO470.h"

int main(int argc, char *argv[]) {

  parser("../given_tests/01/input.json");

  Init();
  

  for (int i = 0; i < 10; i++) {
    Propagate();
  }
  return 0;
}