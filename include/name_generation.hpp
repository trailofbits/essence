#pragma once
#ifndef HANDSANITIZER_NAME_GENERATION_HPP
#define HANDSANITIZER_NAME_GENERATION_HPP

// something about std::vector<std::strings>
/*
 * we encounter a lot of possible recursion that is context sensitive
 * for instance if we have a nested struct like
 * struct Y { int a; }; and struct X { Y i; };
 * And the function has an argument f(X x1, X x2);
 * then ideally all code related to parsing x1 would be visibly correspond to x1, and not x2.
 *
 * The way how we do this is by having vectors/"paths" which keep track of the name at each level.
 * This gives us a very nice property, that we convert a vector<string> -> string through concat with a delimiter guarantees that any two vectors are unique as long as just entry is unique
 * The only extra case we have to check is that there is no predefined name in the module we parse that uses the same delimiter we use to generate names
 */

#include <string>
#include <sstream>
#include <fstream>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>
#include "handsan.hpp"

#include <iostream>

// Any name that is auto-generated must be unique
//
//extern std::string CPP_ADDRESSING_DELIMITER;
//extern std::string LVALUE_DELIMITER;
//extern std::string POINTER_DENOTATION;
//
//
//std::string joinStrings(std::vector<std::string> path, StringJoiningFormat format);



#endif //HANDSANITIZER_NAME_GENERATION_HPP

