typedef struct A {
    int a[5];
} A;

typedef struct B{
    A* a;
    int b[5];
} B;

int direct_struct_return_with_nested_definitions(B b){
    return b.a->a[0];
}