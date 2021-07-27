// assigns global to each other here to observe it this is also correct
int int_pointer_val_arg_second_value_in_array;


void pointer_array_of_ints_test(int* array_of_ints){
    int_pointer_val_arg_second_value_in_array = array_of_ints[1];
    return;
}