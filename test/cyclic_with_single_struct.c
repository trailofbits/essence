typedef struct X{
    struct X* next;
    int a;
} X;

int cyclic_with_single_struct(X* x){
    return x->next->next->a;
}