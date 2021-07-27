// assigns global to each other here to observe it this is also correct
char direct_char_value_number;

void pointer_direct_byte_assignment_test(char* boxed_char){
    direct_char_value_number = *boxed_char;
    return;
}