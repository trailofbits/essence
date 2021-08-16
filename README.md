# Essence

This project aims to extract functions from llvm bitcode files and generate executables for them.
Functions can be specified by name and convenient json input templates will be generated in which their arguments (including globals) can be specified.
The project is sub-devided into two components: `Essence` is the python tool which is meant for users and handles all of the end to end functionality, and `Handsanitizer` which is the c++ component which interacts directly with the LLVM bitcode modules and generates neccesary code/templates.

### About purity
Our focus is primarly "pure" functions. There are two major categories of pure functions

1. `read none`:
   This category returns the same value for the same input every no matter how often the function is called.
   Additionally a `read none` functions are not allowed to have side-effects and hence are not allowed to touch memory at all. 

2. `write only`: These functions are similar to `read none` with the exception that these _are_ allowed to _write_ to memory. Since they are not allowed read from memory the behaviour should still be identical for every identical input, but this class can save for instance the return values to a global variable 




## Install instructions

```shell
$ sudo apt install nlohmann-json3-dev
$ git clone --recursive https://github.com/trailofbits/essence
$ cd essence
$ cmake CMakeLists.txt
$ make 
$ pip install -e ./
```


### Commands  
#### essence <input.bc> 
This command lists the functions in a bitcode file, together with their signature and purity level 


#### essence --build [--output/-o outputdir] [--no-template] <input.bc> f1 f2 ...
This command will build an executable for f1 and f2.
The output directory can be specified, as well as that the json input template should not be (re)generated to preserve arguments inside the template file.


#### essence --build-read-none <input.bc>
Build all functions inside input.bc that are of purity level `read-none`

#### essence --build-write-only <input.bc>
Build all functions inside input.bc that are of purity level `write-only`



## Input routing / output routing
### Input 
To provide arguments to the function for which a standalone executable has been generated we provide convienent to use json input templates. These contain all global variables as well as a complete resprentation of the arguments possible arguments.

Essence also allows for setting the values underlying to values which point to memory.   


example:
```
typedef struct A {
   int v;
} A;


typedef struct B {
   int x;
   int* y;
   struct A* b;
   char * str;
} B;

void f(B b) { return; };

// json input template
{
  "globals": {
  },
  "arguments": {
    "barvictor": {
      "a0": int32_t,
      "a1": int32_t,
      "a2": {
        "a0": int32_t
      },
      "a3": [char]
    }
  }
}
```

If a value is a pointer we support the following inputs:
```json
{
   "int_val_ptr_1": 30, # sets 30 at the addressed referenced by int_val_ptr,
   "int_val_ptr_2": [30, 40, 50], # this will set the memory directly after 30 to 40 and 50 
   "char_ptr_val_1": "a", # for chars we support ascii, note a null will be placed directly after "a",
   "char_ptr_val_2": "a whole string is supported and is null terminated",
   "char_ptr_val_3": ["a"], # alternatively you can box it in an array, this will prevent null termination
   "char_ptr_val_4": 65, # ascii numeric values are also supported
   "char_ptr_val_5": ["abc", 65, "bc", null] # you can mix and match, if you want a null termination with array syntax you add a null suffix
           
}
```

### Output
After the function has been called, we return the output of the function together with all global variables as a json object on std out. 

## Limitations
Currently, essence is unable to handle the following two scenarios 

### Circular dependencies through pointers
While valid C, essence aims to expose a way to get input into the derefenenced locations, however for this case it would cause infinte recursion.
```c
struct A;
struct B;

typedef struct A {
    B* b;    
};

typedef struct B{
    A a;
} B;
```

### Functions that write to the same locations as essence
Essence tries to expose all global variables used in an bitcode module.
If an input module uses variables which also get used internally for essence (like stdio) then we run into the issue that we cannot generate binaries properly anymore.

Note that since essence extracts functions with their used globals beforehand, it is possible to generate a binary for a function  which does not yield a conflict, while there simultaneously exists functions which do generate conflict.


### TODO: Write a piece about recursively extractely functions and purity
