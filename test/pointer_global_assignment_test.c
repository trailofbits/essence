// assigns global to each other here to observe it this is also correct
int* int_pointer_global_val_pre_call;
int int_pointer_global_val_post_call;

void pointer_global_assignment_test(){
    int_pointer_global_val_post_call = *int_pointer_global_val_pre_call;
    return;
}