#pragma once

#include <string>
#include "include/handsan.hpp"

namespace handsanitizer{
    class Module {
    public:
        // we assume that the types here are ordered such that a single never has a dependcy before it
        // and that it is complete, i.e no member of a type defined here is not defined by it self (excluding native types)
        std::string name;
        std::vector<Type*> user_defined_types;
        std::vector<GlobalVariable> globals;
        std::vector<Function> functions;

        std::string getUniqueTmpCPPVariableNameFor(std::vector<std::string> prefixes);
        std::string getUniqueTmpCPPVariableNameFor(std::string prefix);
        std::string getUniqueTmpCPPVariableNameFor();

        void generate_cpp_file_for_function(Function& f, std::string dest_file_path);
        void generate_json_input_template_file(Function& f, std::string dest_file_path);

        void generate_json_module_specification(std::string dest_file_path);

    private:
        std::vector<std::string> definedNamesForFunctionBeingGenerated;

        std::string getTextForUserDefinedTypes();
        std::string getTextForUserDefinedType(Type *type);

        bool isNameDefined(std::string name);
        std::string getRandomDummyVariableName();
        std::string getGlobalDeclarationsText();
        std::string getTypedArgumentNames(Function &f);
        std::string getUntypedArgumentNames(Function &f);
        std::string getParserRetrievalText(std::string jsonInputVariableName, std::vector<handsanitizer::NamedVariable> args, bool isForGlobals = false);
        std::string getParserRetrievalForNamedType(std::string jsonInputVariableName, std::vector<std::string> prefixes, handsanitizer::Type* type, bool isForGlobals = false);
        std::string getParserRetrievalTextForArguments(std::string jsonInputVariableName, std::vector<Argument> args);
        std::string getParserRetrievalTextForGlobals(std::string jsonInputVariableName);

        std::string getJsonOutputText(std::string output_var_name, handsanitizer::Type* retType);
        std::string getJsonOutputForType(std::string json_name, std::vector<std::string> prefixes, handsanitizer::Type* type, bool skipRoot = false);

        std::string getUnrolledTypeAsJson(Type& type);

        std::string registerVariableToBeFreed(std::string variable_name);
        bool freeVectorNameHasBeenSet = false;
        std::string freeVectorVariableName;
        std::string getFreeVectorName();
        std::string getFreeVectorFreeText();

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
    };
}

