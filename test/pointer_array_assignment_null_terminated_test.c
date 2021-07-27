/*
 * we need to check for
 * concat is done correctly
 * null bytes are added correctly
 * strings are not null terminated
 * mixing strings and byte values work properly
 *
 * ["ab", "cd", null, "e", 102, null]
 */
char second_char_of_array; // b
char third_char_of_array; // c
char fifth_char_of_array; // null
char sixth_char_of_array; // e
char seventh_char_of_array; // 102 aka ascii f
char eight_char_of_array; // null


void pointer_array_assignment_null_terminated_test(char* array_null_termed){
    second_char_of_array = array_null_termed[1];
    third_char_of_array = array_null_termed[2];
    fifth_char_of_array = array_null_termed[4];
    sixth_char_of_array = array_null_termed[5];
    seventh_char_of_array = array_null_termed[6];
    eight_char_of_array = array_null_termed[7];

    return;
}