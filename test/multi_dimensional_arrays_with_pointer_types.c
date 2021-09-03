int*** multi_array[3][2];

int multi_dimensional_arrays_with_pointer_types(int*** a[2][3]){
    multi_array[0][0] = a[0][1];
    return ***multi_array[2][1];
}