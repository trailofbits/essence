int global_val_int_a;
char global_val_char_b;

int* global_val_int_p_c;
char* global_val_char_p_d;

void set_values(int a, char b, int* c, char* d){
    global_val_int_a = a;
    global_val_char_b = b;
    global_val_int_p_c = c;
    global_val_char_p_d = d;
    return;
}