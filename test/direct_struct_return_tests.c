#include "stdint.h"

typedef struct X{
    int32_t a[1];
    int32_t b;
    int8_t c;
} X;

X direct_struct_return_test(){
    X x;
    x.a[0] = 1;
    x.b =2;
    x.c = 'a';
    return x;
}