#include "stdint.h"

int32_t array_definition[3];
int32_t before_func_called;
int32_t after_func_called;

void global_tests(int32_t a){
    array_definition[0] = 0;
    array_definition[1] = 1;
    array_definition[2] = 2;
    after_func_called = a;

    return ;
}