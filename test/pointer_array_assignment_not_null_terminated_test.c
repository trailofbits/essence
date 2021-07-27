/*
 * input ["fg"], should not be null terminated
 */
char third_char_of_non_null_terminated_array;

void pointer_array_assignment_not_null_terminated_test(char* array_non_null){
    third_char_of_non_null_terminated_array = array_non_null[1];
    return;
}