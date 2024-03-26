
#include <stdio.h>
#include "../OoO470.h"

int main(int argc, char *argv[]) {

    // get from argv the path to the input file and output file

char *input_file = argv[1];
char *output_file = argv[2];
if (argc != 3) {
    printf("Usage: %s <input_file> <output_file>\n", argv[0]);
    return 1;
}

  parser(input_file);

  Init();
  

  for (int i = 0; i < 10; i++) {
    Propagate();
  }
  return 0;
}