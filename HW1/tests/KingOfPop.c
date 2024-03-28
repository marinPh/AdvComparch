#include "../lib/OoO470.h"

int main(int argc, char *argv[])
{
    printf("Hello, World!\n");
    parser("../given_tests/01/input.json");
    printf("%d", PopFreeList());
    showFreeList();

    return 0;
}