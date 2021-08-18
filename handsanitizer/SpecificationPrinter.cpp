#include "SpecificationPrinter.h"
#include <fstream>
#include <set>
#include <iomanip>
#include <filesystem>

namespace handsanitizer {
    void SpecificationPrinter::printSpecification(std::string outputDir, std::string fileName) {
        std::string outputFilePath = getOutputFilename(outputDir, fileName);
        std::ofstream of;
        of.open(outputFilePath, std::ofstream::out | std::ofstream::trunc);

        std::stringstream output;

        output << "{" << std::endl;
        printGlobalsToOutputStream(output);
        printFunctionsToOutputStream(output);
        output << "}" << std::endl;


        of << std::setw(4) << output.str();
        of.close();
    }

    std::string
    SpecificationPrinter::getOutputFilename(const std::string &outputDir, const std::string &fileName) const {
        std::filesystem::path p(fileName);
        p.replace_extension("");
        auto newFileName = p.filename().string();
        auto outputPath = std::filesystem::path();
        outputPath.append(outputDir);
        outputPath.append(newFileName);
        outputPath.replace_extension("spec.json");
        auto outputFilePath = outputPath.string();
        return outputFilePath;
    }

    void SpecificationPrinter::printFunctionsToOutputStream(std::stringstream &output) {
        output << "\"functions\": [" << std::endl;
        for (auto &f: functions) {
            output << "{";
            output << "\"name\": \"" << f.function->name << "\"," << std::endl;
            output << "\"signature\": \"" << f.function->getFunctionSignature() << "\"," << std::endl;
            output << "\"purity\": \"" << f.function->getPurityName() << "\"," << std::endl;
            output << "\"arguments\": [" << std::endl;
            for (auto &a : f.function->arguments) {
                output << "{\"" << a.getName() << "\"" << ": " << getUnrolledTypeAsJson(*a.getType(), std::vector<std::pair<Type *, std::string>>()) << "}";
                if (a.getName() != f.function->arguments.back().name)
                    output << ",";
            }
            output << "]}";
            if (f.function->name != functions.back().function->name)
                output << ",";

        }
        output << "]" << std::endl; // many possible functions
    }

    void SpecificationPrinter::printGlobalsToOutputStream(std::stringstream &output)  {
        std::vector<GlobalVariable> globals = getSetOfUniqueGlobalVariables();
        output << "\"globals\": {" << std::endl;
        for (auto &g : globals) {
            output << "\"" << g.getName() << "\"" << ": " << getUnrolledTypeAsJson(*g.getType(), std::vector<std::pair<Type *, std::string>>());
            if (g.getName() != globals.back().name)
                output << ",";
        }
        output << "}," << std::endl;
    }

    std::vector<GlobalVariable> SpecificationPrinter::getSetOfUniqueGlobalVariables() {
        auto cmp = [](const GlobalVariable &a, const GlobalVariable &b) -> bool {
            return a.name.compare(b.name) < 0;
        };
        std::set<GlobalVariable, decltype(cmp)> s(cmp);

        for (auto &func: functions) {
            for (auto &global : func.getGlobals())
                s.insert(global);
        }

        std::vector<GlobalVariable> all_globals;
        all_globals.assign(s.begin(), s.end());
        return all_globals;
    }
}