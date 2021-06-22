# HandSanitizer

This project aims to do the following to extract all pure functions from an llvm bitcode file, and emit a separate executable for each.
Function arguments may be specified from the cli 

mvp example 

```c
llvm containing bitcode for: 
int f(int x, int y){ return x + y}

hsan identify input.bc
f(int x, int y)

hsan generate input.bc
--- wrote input.b ---
./input.b --x=1 --y=2
```

