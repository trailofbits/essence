#include <stdlib.h>

int array_of_ints[3];
int* array_of_pointers[3];

void output_contains_global_array_values_tests(){
    array_of_ints[0] = 1;
    array_of_ints[1] = 2;
    array_of_ints[2] = 3;

    array_of_pointers[0] = malloc(sizeof(int));
    array_of_pointers[1] = malloc(sizeof(int));
    array_of_pointers[2] = malloc(sizeof(int));
}