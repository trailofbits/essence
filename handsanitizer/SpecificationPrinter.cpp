#include "SpecificationPrinter.h"
#include <fstream>
#include <set>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace handsanitizer {
    nlohmann::json SpecificationPrinter::getUnrolledTypeAsJson(Type &type, std::vector<std::pair<Type *, std::string>> typePath) {
        nlohmann::json json;

        json["type"] = type.getTypeName();
        if(type.isVoidTy()){
            // do nothing
        }
        if (type.isIntegerTy())
            json["bitWidth"] = type.getBitWidth();

        if (type.isPointerTy())
            json["pointerElementType"] = getUnrolledTypeAsJson(*type.getPointerElementType(), typePath);

        if (type.isArrayTy()) {
            json["getArrayNumElements"] = type.getArrayNumElements();
            json["arrayElementType"] = getUnrolledTypeAsJson(*type.getArrayElementType(), typePath);
        }

        if (type.isStructTy()) {
            json["structName"] = type.getCTypeName();
            std::vector<nlohmann::json> members;
            for (auto &member: type.getNamedMembers()) {
                nlohmann::json memberJson;

                auto firstOccurrenceOfType = std::find_if(typePath.begin(), typePath.end(), [member](const std::pair<Type*, std::string>& pair){ return member.type == pair.first;});
                if(firstOccurrenceOfType == typePath.end()){
                    std::stringstream path;
                    for(auto& typeInPath : typePath)
                        path << "[\'" + typeInPath.second + "\']";
                    memberJson[member.getName()] = "cyclic_with_" + path.str();
                }
                else{
                    auto memberTypePath(typePath);
                    memberTypePath.emplace_back(member.getType(), member.getName());
                    memberJson[member.getName()] = getUnrolledTypeAsJson(*member.getType(), memberTypePath);
                }
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
            std::vector<std::pair<Type *, std::string>> typePath;
            typePath.emplace_back(g.getType(), g.getName());
            json["globals"][g.getName()] = getUnrolledTypeAsJson(*g.getType(), typePath);
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
                std::vector<std::pair<Type *, std::string>> typePath;
                typePath.emplace_back(a.getType(), a.getName());
                argJson["type"] = getUnrolledTypeAsJson(*a.getType(), typePath);
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