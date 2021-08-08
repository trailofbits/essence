int global_used = 0;
int global_unused = 0;

void recursively_stripped(){
    global_used = 1;
}

void stripped_function(){
    recursively_stripped();
    return;
}