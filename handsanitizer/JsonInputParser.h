#pragma once
#include <string>
#include <memory>
#include <utility>
#include "DeclarationManager.h"

namespace handsanitizer{
    struct JsonParsingCandidate{
        /*
         * lvalue is REQUIRED to be a valid name
         */
        std::string lvalueName;
        std::string jsonRoot; // the expected object to be extracted, for non-aggregate this would be the scalar otherwise the array/struct
        Type* type; // the type of the object to be extracted
        bool isForGlobalVariable; // in case of globals
    };

class JsonInputParser {
public:
    explicit JsonInputParser(std::shared_ptr<DeclarationManager> declarationManager) : declarationManager(std::move(declarationManager)) {};

    std::string getStructParsingHelpers();
    std::string getParserRetrievalTextForArguments(std::string jsonInputVariableName, std::vector<Argument> args);
    std::string getParserRetrievalTextForGlobals(std::string jsonInputVariableName);
private:
    std::shared_ptr<DeclarationManager> declarationManager;
    std::vector<std::pair<Type*, std::string>> structParsingHelperFunctions;
    std::string getParserRetrievalText(const std::string& jsonInputVariableName, const std::vector<handsanitizer::NamedVariable>& args, bool isForGlobals = false);
    std::string getParserRetrievalForNamedType(const JsonParsingCandidate &candidate);

    std::string getPointerToStructParserFunctionDefinitions(Type* type);
    /*
     *
        scalar
        struct
        globalArray
        pointer_to_pointer
        pointer_to_non_pointer
            i8 vs rest
     */
    static std::string getParserRetrievalForScalars(const JsonParsingCandidate &candidate);
    std::string getParserRetrievalForStructs(const JsonParsingCandidate &candidate, bool isMalloced);
    std::string getParserRetrievalForGlobalArrays(const JsonParsingCandidate &candidate);
    std::string getParserRetrievalForPointers(const JsonParsingCandidate &candidate);
    std::string getParserRetrievalForPointersToNonPointers(const JsonParsingCandidate &candidate);
    std::string getParserRetrievalForPointersToPointers(const JsonParsingCandidate &candidate);
    std::string getParserRetrievalForPointerToCharType(const JsonParsingCandidate &candidate);
    std::string getParserRetrievalForPointerToNonCharType(const JsonParsingCandidate &candidate);

    static std::string getStringFromJson(const std::string& output_var, const std::string& json_name, const std::string& tmp_name_1, const std::string& tmp_name_2, bool addNullTermination = true);
    static std::string getStringLengthFromJson(const std::string& output_var, const std::string& json_name, const std::string& tmp_name_1, const std::string& tmp_name_2, bool addNullTermination = true);
};
}