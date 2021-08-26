#include "JsonOutputGenerator.h"

namespace handsanitizer{
    std::string JsonOutputGenerator::getJsonOutputForType(const std::string& json_name, const std::vector<std::string>& prefixes, Type *type, bool skipRoot) {
        std::stringstream output;
        if (type->isStructTy()) {
            for (auto &mem : type->getNamedMembers()) {
                std::vector<std::string> member_prefixes(prefixes);
                member_prefixes.push_back(mem.getName());
                output << getJsonOutputForType(json_name, member_prefixes, mem.getType(), skipRoot) << std::endl;
            }
        } else {
            output << json_name;
            if (skipRoot)
                output << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING_WITHOUT_ROOT);
            else
                output << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING);

            if (type->isPointerTy())
                output << " = (uint64_t)";
            else
                output << " = ";

            output << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_ADDRESSING) << ";" << std::endl;
        }
        return output.str();
    }

    // assume output_var_name contains the return value already set
    std::string JsonOutputGenerator::getJsonOutputText(const std::string& output_var_name, handsanitizer::Type *retType) {
        std::stringstream s;
        auto jsonVarName = this->declarationManager->getUniqueTmpCPPVariableNameFor("output_json");
        s << "nlohmann::json " << jsonVarName << ";" << std::endl;
        std::vector<std::string> prefixes;
        for (auto &global : this->declarationManager->globals) {
            prefixes.clear();
            prefixes.push_back(global.getName());
            s << getJsonOutputForType(jsonVarName + "[\"globals\"]", prefixes, global.getType());
        }

        prefixes.clear();
        prefixes.push_back(output_var_name);
        if (!retType->isVoidTy())
            s << getJsonOutputForType(jsonVarName + "[\"output\"]", prefixes, retType, true);


        s << "std::cout << std::setw(4) << " << jsonVarName << " << std::endl;" << std::endl;
        return s.str();
    }
}