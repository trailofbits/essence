typedef struct A {
    int v;
} A;


typedef struct B {
    int x;
    int *y;
    struct A *b;
    char *str;
} B;

int pointer_in_struct_test(B b) {
    return b.b->v;
};