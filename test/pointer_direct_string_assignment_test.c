/* just check if the values inside the array are placed in the correct locations */
/* ["ab"] */
char second_char_value_string;
char third_char_of_string; // should be null term'd


void pointer_direct_string_assignment_test(char* direct_string){
    second_char_value_string = direct_string[1];
    third_char_of_string = direct_string[2];

    return;
}