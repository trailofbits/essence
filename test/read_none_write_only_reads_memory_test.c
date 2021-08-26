int global_val;

void read_none(){
    return;
}

void write_only(){
    global_val =1;
    return;
}

void reads_memory(){
    int x= global_val;
    x++;
    global_val = x;
    return;
}