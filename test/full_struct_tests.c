#include "stdint.h"

typedef union Z{
    int d;
    char e;
}Z;

typedef struct Y{
    int c;
    Z z;
} Y;

typedef struct X{
    int a[3];
    int b;
    Y y;
} X;

X a;
X full_struct_tests(X x){
    x.a[0] = 1;
    x.a[1] = 2;
    x.a[2] = 3;
    x.b = 4;
    x.y.c = 5;
    x.y.z.d = 2;
    a = x;
    return x;
}