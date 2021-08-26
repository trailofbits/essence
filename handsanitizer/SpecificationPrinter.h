#pragma once

#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>
#include "FunctionCallerGenerator.h"


namespace handsanitizer {
    class SpecificationPrinter {
    public:
        explicit SpecificationPrinter(std::vector<handsanitizer::FunctionCallerGenerator> &functions) : functions(functions) {};
        void printSpecification(const std::string& outputDir, const std::string& fileName);

    private:
        std::vector<handsanitizer::FunctionCallerGenerator> &functions;
        std::vector<GlobalVariable> getSetOfUniqueGlobalVariables();

        static std::filesystem::path getPathToOutputFile(const std::string &outputDir, const std::string &fileName) ;
        nlohmann::json getUnrolledTypeAsJson(Type &type, std::vector<std::pair<Type *, std::string>> typePath);
    };
}