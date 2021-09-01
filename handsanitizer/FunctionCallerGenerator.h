#pragma once

#include <string>
#include <utility>
#include <nlohmann/json.hpp>
#include "handsan.hpp"
#include "JsonInputParser.h"
#include "JsonOutputGenerator.h"
#include "DeclarationManager.h"

namespace handsanitizer{
    /*
     * This class represents all parts required to generate an executable for a LLVM function
     * Most notably this is also responsible for the declaration manager which requires a unique instance per instance of this class
     * While an llvm input module has a single set of globals, the functions that will be used as input to this function might be extracted
     * and extract functions might depend on different sets of globals and hence this set might be different
     */
    class FunctionCallerGenerator {
    public:
        std::unique_ptr<Function> function; // The function for we which we generate cpp/json

        FunctionCallerGenerator(std::unique_ptr<Function> function, std::shared_ptr<DeclarationManager> dm)
                : function(std::move(function)) {
            declarationManager = std::move(dm);
            jsonInputParser = std::make_unique<JsonInputParser>(declarationManager);
            jsonOutputGenerator = std::make_unique<JsonOutputGenerator>(declarationManager);
        }

        void generate_cpp_file_for_function(const std::string& dest_file_path);
        void generate_json_input_template_file(const std::string& dest_file_path);

        std::vector<GlobalVariable> getGlobals();

    private:
        std::unique_ptr<JsonInputParser> jsonInputParser;
        std::unique_ptr<JsonOutputGenerator> jsonOutputGenerator;
        std::shared_ptr<DeclarationManager> declarationManager;

        std::string getTextForUserDefinedTypes();
        static std::string getTextForUserDefinedType(Type *type);


        static std::string getUntypedArgumentNames(Function &f);

        nlohmann::json getJsonInputTemplateTextForJsonRvalueAsJson(Type &arg, std::vector<std::pair<Type *, std::string>> typePath, int pointerIndirections = 0);
        std::string getGlobalDeclarationsText();
        std::string getFreeVectorFreeText() ;
        static std::string getMainText();

        void addMetaDataForPointerToJson(nlohmann::json& json, Type& arg);
        void addMetaDataForTypeToJson(nlohmann::json& json, Type& arg);
        void addMetaDataForTypeToJson(nlohmann::json& json, Type& arg, int pointerIndirections);
    };
}
