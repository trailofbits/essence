#include <stdio.h>
typedef struct X {
    int a[5];
    int* b ;
    char* c;
} X;



char testf(X x, int* b){
    return x.c[2];
}


/*
 * example input json file
 *
{
  "e_0": {
    "e_0": [70,71,72,73,74],
    "e_1": 684,
    "e_2": [null , "b"]
  },
  "e_1": [80]
}
 */