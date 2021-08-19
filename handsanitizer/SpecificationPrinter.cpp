#include "SpecificationPrinter.h"
#include <fstream>
#include <set>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace handsanitizer {
    nlohmann::json SpecificationPrinter::getUnrolledTypeAsJsonJson(Type &type) {
        nlohmann::json json;

        json["type"] = type.getTypeName();
        if(type.isVoidTy()){
            // do nothing
        }
        if (type.isIntegerTy())
            json["bitWidth"] = type.getBitWidth();

        if (type.isPointerTy())
            json["pointerElementType"] = getUnrolledTypeAsJsonJson(*type.getPointerElementType());

        if (type.isArrayTy()) {
            json["getArrayNumElements"] = type.getArrayNumElements();
            json["arrayElementType"] = getUnrolledTypeAsJsonJson(*type.getArrayElementType());
        }

        if (type.isStructTy()) {
            json["structName"] = type.getCTypeName();
            std::vector<nlohmann::json> members;
            for (auto &member: type.getNamedMembers()) {
                nlohmann::json memberJson;
                memberJson[member.getName()] = getUnrolledTypeAsJsonJson(*member.getType());
                members.push_back(memberJson);
            }
            json["structMembers"] = members;
        }
        return json;
    }

    void SpecificationPrinter::printSpecification(const std::string& outputDir, const std::string& fileName) {
        std::string outputFilePath = getOutputFilename(outputDir, fileName);
        std::ofstream of;
        of.open(outputFilePath, std::ofstream::out | std::ofstream::trunc);

        nlohmann::json json;

        std::vector<GlobalVariable> globals = getSetOfUniqueGlobalVariables();
        for (auto &g : globals) {
            json["globals"][g.getName()] = getUnrolledTypeAsJsonJson(*g.getType());
        }

        std::vector<nlohmann::json> functionsJson;
        for (auto &f: functions) {
            nlohmann::json functionJson;
            functionJson["name"] = f.function->name;
            functionJson["signature"] = f.function->getFunctionSignature();
            functionJson["purity"] = f.function->getPurityName();
            std::vector<nlohmann::json> argumentsJson;
            for (auto &a : f.function->arguments) {
                nlohmann::json argJson;
                argJson["name"] = a.getName();
                argJson["type"] = getUnrolledTypeAsJsonJson(*a.getType());
                argumentsJson.push_back(argJson);
            }
            functionJson["arguments"] = argumentsJson;
            functionsJson.push_back(functionJson);

        }
        json["functions"] = functionsJson;

        of << json.dump(4) << std::endl;
        of.close();
    }


    std::string
    SpecificationPrinter::getOutputFilename(const std::string &outputDir, const std::string &fileName) {
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