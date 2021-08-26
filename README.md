# Essence

This project aims to generate stand-alone executables for functions residing in llvm bitcode modules.
Functions can be specified by name and convenient JSON input templates will be generated in which their arguments (including globals) can be specified.
The project is sub-divided into two components: `essence` is the python tool which is meant for users and handles all of the end-to-end functionality, and `handsanitizer` which is the c++ component that interacts directly with the LLVM bitcode modules and generates neccesary code/templates.

![Demo gif](demo.gif)

### Features:
The project currently supports:
* Listing all function signatures inside an llvm bitcode module sorted by purity
* Generating executables for a specified list of functions
  * Easy JSON in/output. 
  abstracts memory away allowing the user to specify values directly underneath the pointers as well as directly past it. 
  * Generates minimal templates, global variables not touched by the function are not included 
* Supports aggregate types `arrays`, `structs` and `unions`
* Supports circular pointer type definitions, i.e `struct X { struct X* x};`
* Supports functions calling other functions 


It does _not_ support:
* Purity discovery, meaning that if your program is compiled with -O0 all functions will be listed as impure
* Bitcode modules containing one of the following LLVM types `Function`,`Vector` `x86amx`, or `x86_mmx`
* Functions that use `stdin`/`stdout`



## Install instructions

```shell
$ sudo apt install llvm-11 llvm-11-tools
$ git clone https://github.com/trailofbits/essence
$ cd essence
$ cmake CMakeLists.txt
$ make 
$ pip install -e ./
```
Or as a one-liner
```shell
git clone https://github.com/trailofbits/essence && cd essence && cmake CMakeLists.txt && make && pip install -e ./
```


it is required to specify the `-e` flag as otherwise pathing between `essence` and `handsanitizer` will break.


## Commands
#### `essence <input.bc>`
This command lists the functions in a bitcode file, together with their signature and purity level


#### `essence --build [--output/-o outputdir] [--no-template] <input.bc> <list_of_function_names>`
This command will build an executable for f1 and f2.
The output directory can be specified, as well as that the JSON input template should not be (re)generated to preserve arguments inside the template file.


#### `essence --build-read-none <input.bc>`
Build all functions inside input.bc that are of purity level `read-none`

#### `essence --build-write-only <input.bc>`
Build all functions inside input.bc that are of purity level `write-only`

### About purity
Our focus is primarily "pure" functions. There are two major categories of pure functions

1. `read none`:
   This category returns the same value for the same input every no matter how often the function is called.
   Additionally, a `read none` functions are not allowed to have side-effects and hence are not allowed to touch memory at all.

2. `write only`: These functions are similar to `read none` with the exception that these _are_ allowed to _write_ to memory. Since they are not allowed to read from memory the behavior should still be identical for every identical input, but this class can save for instance the return values to a global variable



## Input routing / output routing
### Input
To provide arguments to the function for which a standalone executable has been generated we provide convenient to use JSON input templates. These contain all global variables as well as a complete representation of the arguments.

`essence` also allows for setting the values to which the pointer points.


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

#### String routing
If a value is a pointer we support the following inputs:
```json
{
   "int_val_ptr_1": 30, // sets 30 at the addressed referenced by int_val_ptr,
   "int_val_ptr_2": [30, 40, 50], // this will set the memory directly after 30 to 40 and 50 
   "char_ptr_val_1": "a", // for chars we support ascii, note a null will be placed directly after "a",
   "char_ptr_val_2": "a whole string is supported and is null terminated",
   "char_ptr_val_3": ["a"], // alternatively you can box it in an array, this will prevent null termination
   "char_ptr_val_4": 65, // ascii numeric values are also supported
   "char_ptr_val_5": ["abc", 65, "bc", null] // you can mix and match, if you want a null termination with array syntax you add a null suffix
}
```

#### Routing over Cyclic definitions
Consider the example of an linked list

```c
typedef struct linked_list {
    struct linked_list* next;
    int value;
};

int print_list_value_of_linked_list(linked_list ll){
    if(ll->next != NULL)
        return print_list_value_of_linked_list(next);
    return ll.value;
}
```

Essence with its input templates exposes a mechanism to set the underlying value of pointers and thus also to `linked_list.next`.
This recurses infinitely, we therefore have a special field within the specification and input template denoting how it cycles and provide an easy way to generate desired recursive input.

```json
{
   "arguments": {
      "barvictor": {
         "a0": {
            "a0": "cycles_with_['barvictor']['a0']",
            "a1": "int32_t"
         },
         "a1": "int32_t"
      }
   },
   "globals": {}
}

```
The `cyclic_with_<path_to_object>` denotes that the entire object located at `<path>` may be copy-pasted at the location an arbitrary amount of times. With the requirement that it terminates by a `null` value.

```json
{
   "arguments": {
      "barvictor": {
         "a0": {
            "a0": {
               "a0": {
                  "a0": null,
                  "a1": 4
               },
               "a1": 3
            },
            "a1": 2
         },
         "a1": 1
      }
   },
   "globals": {}
}

```


### Output
After the function has been called, we return the output of the function together with all global variables as a JSON object on std out.





## Limitations
Currently, `essence` is unable to handle the following

### Function types
If any function type is present in the binary `essence` has to halt its analysis.
When this causes problems for unrelated functions the user should first extract the relevant parts of the module out.



### Functions that write to the same locations as `essence`
`essence` tries to expose all global variables used in a bitcode module.
If an input module uses variables that also get used internally for `essence` (like stdio) then we run into the issue that we cannot generate binaries properly anymore.

Note that since `essence` extracts functions with their used globals beforehand, it is possible to generate a binary for a function that does not yield a conflict, while there simultaneously exist functions that do generate conflict.

