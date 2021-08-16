#pragma once
#include <string>
#include <memory>
#include "DeclarationManager.h"

namespace handsanitizer{

class JsonInputParser {
public:
    explicit JsonInputParser(std::shared_ptr<DeclarationManager> declarationManager) : declarationManager(declarationManager) {};

    std::string getParserRetrievalTextForArguments(std::string jsonInputVariableName, std::vector<Argument> args);
    std::string getParserRetrievalTextForGlobals(std::string jsonInputVariableName);
private:
    std::shared_ptr<DeclarationManager> declarationManager;
    std::string getParserRetrievalText(std::string jsonInputVariableName, std::vector<handsanitizer::NamedVariable> args, bool isForGlobals = false);
    std::string getParserRetrievalForNamedType(std::string jsonInputVariableName, std::vector<std::string> prefixes, handsanitizer::Type* type, bool isForGlobals = false);

    /*
     *
        scalar
        struct
        globalArray
        pointer_to_pointer
        pointer_to_non_pointer
            i8 vs rest
     */
    std::string getParserRetrievalForScalars(std::string jsonInputVariableName, std::vector<std::string> prefixes, handsanitizer::Type *type, bool isForGlobals);
    std::string getParserRetrievalForStructs(std::string jsonInputVariableName, std::vector<std::string> prefixes, handsanitizer::Type *type, bool isForGlobals);
    std::string getParserRetrievalForGlobalArrays(std::string jsonInputVariableName, std::vector<std::string> prefixes, handsanitizer::Type *type);
    std::string getParserRetrievalForPointers(std::string jsonInputVariableName, std::vector<std::string> prefixes, handsanitizer::Type *type, bool isForGlobals);
    std::string getParserRetrievalForPointersToNonPointers(std::string jsonInputVariableName, std::vector<std::string> prefixes, handsanitizer::Type *type, bool isForGlobals);
    std::string getParserRetrievalForPointersToPointers(std::string jsonInputVariableName, std::vector<std::string> prefixes, handsanitizer::Type *type, bool isForGlobals);
    std::string getParserRetrievalForPointerToCharType(std::string jsonInputVariableName, std::vector<std::string> prefixes, handsanitizer::Type *type, bool isForGlobals);
    std::string getParserRetrievalForPointerToNonCharType(std::string jsonInputVariableName, std::vector<std::string> prefixes, handsanitizer::Type *type, bool isForGlobals);

    std::string getStringFromJson(std::string output_var, std::string json_name, std::string tmp_name_1, std::string tmp_name_2, bool addNullTermination = true);
    std::string getStringLengthFromJson(std::string output_var, std::string json_name, std::string tmp_name_1, std::string tmp_name_2, bool addNullTermination = true);
};
}