#pragma once

#include <vector>
#include "FunctionCallerGenerator.h"


namespace handsanitizer {
    class SpecificationPrinter {
    public:
        SpecificationPrinter(std::vector<handsanitizer::FunctionCallerGenerator> &functions) : functions(functions) {};
        void printSpecification(std::string outputDir, std::string fileName);

    private:
        std::vector<handsanitizer::FunctionCallerGenerator> &functions;

        std::vector<GlobalVariable> getSetOfUniqueGlobalVariables();
        void printGlobalsToOutputStream(std::stringstream &output);
        void printFunctionsToOutputStream(std::stringstream &output);

        std::string getOutputFilename(const std::string &outputDir, const std::string &fileName) const;
    };
}