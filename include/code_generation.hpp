#pragma once
#ifndef HANDSANITIZER_CODE_GENERATION_HPP
#define HANDSANITIZER_CODE_GENERATION_HPP

#include <string>
#include <sstream>
#include <llvm/IR/Type.h>
#include "handsan.hpp"

// note that the functions using this name should handle path construction so don't suffix this with /
extern std::string OUTPUT_DIR;


std::string DeclareVariable(std::string name, llvm::Type* type);
std::string GetLValue(std::string name);

std::string OpenForLoop(std::string iterator_name, std::string size);
std::string CloseForLoop(); // we could make it also get name and string size to test whether code-gen is ok

std::string OpenIfStatement(std::string expression);
std::string CloseIfStatement(); // we could make it also get name and string size to test whether code-gen is ok


std::string GetMallocAndAddFree(std::string retval_name, std::string size);

std::string DeclareReservedGlobals();



std::string getStringFromJson(std::string output_var, std::string json_name, std::string tmp_name_1, std::string tmp_name_2, bool addNullTermination = true);
std::string getStringLengthFromJson(std::string output_var, std::string json_name, std::string tmp_name_1, std::string tmp_name_2, bool addNullTermination = true);


#endif //HANDSANITIZER_CODE_GENERATION_HPP
