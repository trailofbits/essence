#ifndef HANDSANITIZER_ARRAYTEST_C
#define HANDSANITIZER_ARRAYTEST_C

typedef struct Y{
    int a[5];
} Y;

typedef struct X{
    Y y[3];
} X;

int testf(X x){
    int counter = 0 ;
    for(int i = 0; i < 3; i++){
        for(int j = 0; j < 5; j++){
            counter = counter + x.y[i].a[j];
        }
    }
    return counter;
}

#endif //HANDSANITIZER_ARRAYTEST_C
