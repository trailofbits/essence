#pragma once
#include <string>
#include <vector>
#include "handsan.hpp"

namespace handsanitizer{
    enum StringJoiningFormat{
        GENERATE_FORMAT_CPP_ADDRESSING,
        GENERATE_FORMAT_CPP_VARIABLE,
        GENERATE_FORMAT_JSON_ARRAY_ADDRESSING,
        GENERATE_FORMAT_JSON_ARRAY_ADDRESSING_WITHOUT_ROOT
    };

    extern const std::string CPP_ADDRESSING_DELIMITER;
    extern const std::string LVALUE_DELIMITER;
    extern const std::string POINTER_DENOTATION;


    /*
     * This class represents all of the defined symbols for a generated function
     * This class:
     *      1. is unique per generated function
     *      2. is responsible for generating unique names in the generated c++ code for the function
     *      3. needs to keep track of type definitions
     *      4. needs to be constructed before the generated function as generation depends on stuff like pre-existing globals
     */

class DeclarationManager{
public:
    [[nodiscard]] bool isNameDefined(std::string name) const;
    void addDeclaration(Type *f);
    void addDeclaration(const GlobalVariable &gv);
    void addDeclaration(const std::string &reserve_name);

    void clearGeneratedNames();

    std::string joinStrings(const std::vector<std::string>& strings, StringJoiningFormat format) const;

    std::string getUniqueTmpCPPVariableNameFor(const std::string& candidate);
    std::string getUniqueTmpCPPVariableNameFor();
    std::string getUniqueLoopIteratorName();
    std::string registerVariableToBeFreed(const std::string& variable_name);

    std::string getFreeVectorName();


    /*
     * Generators need theses
     */
    std::vector<GlobalVariable> globals;
    std::vector<Type*> userDefinedTypes;
private:
    std::vector<std::string> definedNamesForFunctionBeingGenerated;

    /*
     * Iterator names are need to be tracked separate as indices are addressed different if they iterator or not
     * i.e if we have an array of structs inside a struct we require the following
     * inputJson["top_level_struct"][iterator]["member_struct"]
     * Notice that the quotes are left out if they are iterators
     */
    std::vector<std::string> iteratorNames;
    /*
     * There might still be a set of symbols in an extracted module that are not simply function name + globals
     * As even read_none functions can call other read_none functions, and these functions are also contained in the extracted module
     * Therefore we have a misc like set which captures those.
     */
    std::vector<std::string> otherDisallowedNames;
    bool freeVectorNameHasBeenSet = false;
    std::string freeVectorVariableName;
    static const std::string& getRandomDummyVariableName();
};

};