# HandSanitizer

This project aims to do the following to extract all pure functions from an llvm bitcode file, and emit a separate executable for each.
Function arguments may be specified from the cli 

mvp example 

```c
llvm containing bitcode for: 
int f(int x, int y){ return x + y}
~~~~
hsan identify input.bc
f(int x, int y)

hsan generate input.bc
--- wrote input.b ---
./input.b --x=1 --y=2
```



# parsing

a= 64 => a is the value 64, this is pulled through stio 
b='64' b is the string 64 




# Argument usage
we use llvm arguments class as we need to know whether values have attributes like passByVal. 



assumptions 
first all balues are parsed ST args have pos+type

structs need to be defined, moreover the member names need to be accessible after parsing
in setup, and assignment before func call

when a struct is parsed during the first discobery of the target function we have 
that members 


we can use stack allocated members with & to deal with passbyref, just make sure this isn't done for large arrays



# Pointers
For pointers we do the following:
1. If a function does accept a pointer, either directly or indirectly as a struct member, 
we expose a setter to the underlying value, regardless of how many levels of indirections there are
   
2. Scalar and struct types are placed on the stack, arrays on the heap.


Example 1: Setting the underlying value
```
void f(int* x);
int x = parser.get<int>("--x"); // set the underlying value
f(&x);
```

Example 2: Multiple levels of indirection exist 
```
f(char*** z);
char u = parser.get<char>("--z");
char x = &u;
char** y = &x;
char*** z = &y; 
f(z); // or f(&y);
```

Later on we might include support aliasing, but this feature is currently considered out of scope. An example might be following scenario: 
```
f(int** x, int *y);
./handsanitzer --x=3 y=--x

int* x2 = &parser.get<char>("--x");
int** x1 = &x2;
int* y = x2;
f(x1, y);
```


# Naming conventions
As our approach is code generation based, encounter that naming needs to be done in specific contexts in different ways.

The three areas that work based on naming conventions are:


argparse/cli naming
c++ struct definitions and member names
c++ variable names
 

### argparse/cli 
Our program interface looks like the following:
`./handsanitizer --x val1 --y.z val2 `
the arguments are a tree from the root argument variable of the function, with each struct member relationship being delimited with a `.`
so 
```
typedef struct Y{ int x; };
typedef struct S{ int a; Y y };
void f(S s);
./handsanitzer --s.a val1 --s.y.x val2  // val1 val2 are any integer 
```

Pointers aren't exposed in the CLI, but are implicitly handled 

### c++ struct definitions and member names
We just try to make pretty names for these things, this is very much W.I.P but we make sure that all struct names are unique and accesible

### c++ variable names
For structs we have
```
//get m1 above like any other type
<struct type> st {
    .m1 = m1,
    .m2 = m2, 
    ...
};
```

Pointers are a pain due to there being no easy construct to represent as there is no construct for taking addresses of rvalues in an unnamed fashion
Therefore we require that any `T*** val` translates to: 
```
T val3 = <value of type T> 
T* val2 = &val3;
T** val1 = &val2;
T*** val = &val1; //
```

