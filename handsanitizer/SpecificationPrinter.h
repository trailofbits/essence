#pragma once

#include <vector>
#include <nlohmann/json.hpp>
#include "FunctionCallerGenerator.h"


namespace handsanitizer {
    class SpecificationPrinter {
    public:
        SpecificationPrinter(std::vector<handsanitizer::FunctionCallerGenerator> &functions) : functions(functions) {};
        void printSpecification(const std::string& outputDir, const std::string& fileName);

    private:
        std::vector<handsanitizer::FunctionCallerGenerator> &functions;

        std::vector<GlobalVariable> getSetOfUniqueGlobalVariables();

        static std::string getOutputFilename(const std::string &outputDir, const std::string &fileName) ;
        nlohmann::json getUnrolledTypeAsJson(Type &type, std::vector<std::pair<Type *, std::string>> typePath);
    };
}