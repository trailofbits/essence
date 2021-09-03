#include <iostream>
#include <fstream>
#include "FunctionCallerGenerator.h"
#include <nlohmann/json.hpp>

namespace handsanitizer {
    void FunctionCallerGenerator::generate_cpp_file_for_function(const std::string &dest_file_path) {
        declarationManager->clearGeneratedNames();
        std::stringstream setupFileStream;

        setupFileStream << "#include <cstdint>" << std::endl;
        setupFileStream << "#include <iomanip>" << std::endl;
        setupFileStream << "#include <argparse/argparse.hpp>" << std::endl;
        setupFileStream << "#include <fstream>" << std::endl;
        setupFileStream << "#include <nlohmann/json.hpp> // header only lib" << std::endl << std::endl;


        // DECLARE STRUCTS
        setupFileStream << getTextForUserDefinedTypes();
        setupFileStream << "std::vector<void*> " << declarationManager->getFreeVectorName() << ";" << std::endl;

        setupFileStream << jsonInputParser->getStructParsingHelpers();

        // DECLARATION TEST FUNCTION
        // the LLVM type name differs from how we define it in our generated source code
        std::string functionSignature = function->getFunctionSignature();
        setupFileStream << "extern \"C\" " + functionSignature << std::endl;

        // DECLARE GLOBALS

        setupFileStream << "argparse::ArgumentParser parser;" << std::endl << std::endl;
        setupFileStream << "// globals from module" << std::endl << getGlobalDeclarationsText() << std::endl
                        << std::endl;


        // DEFINE SETUPPARSER
        setupFileStream << "void setupParser() { " << std::endl
                        << "std::stringstream s;"
                        << "s << \"Test program for: " << functionSignature << R"(" << std::endl << R"""()"
                        << getTextForUserDefinedTypes() << ")\"\"\";" << std::endl
                        << "\tparser = argparse::ArgumentParser(s.str());" << std::endl
                        << "} " << std::endl << std::endl;

        // DEFINE CALLFUNCTION
        setupFileStream << "void callFunction() { " << std::endl
                        << "\t\t" << "std::string inputfile = parser.get<std::string>(\"input_file\");" << std::endl;
        auto inputFileName = declarationManager->getUniqueTmpCPPVariableNameFor("inputFile");
        auto jsonInputVariable = declarationManager->getUniqueTmpCPPVariableNameFor("j");
        setupFileStream << "\t\t" << "std::ifstream " << inputFileName << "(inputfile);\n"
                                                                          "\t\tnlohmann::json " << jsonInputVariable
                        << ";" << std::endl
                        << inputFileName
                        << " >> "
                        << jsonInputVariable << ";" << std::endl

                        << jsonInputParser->getParserRetrievalTextForGlobals(jsonInputVariable)
                        << jsonInputParser->getParserRetrievalTextForArguments(jsonInputVariable, function->arguments);

        auto output_var_name = declarationManager->getUniqueTmpCPPVariableNameFor("output");
        if (function->retType->isVoidTy() == false)
            setupFileStream << "auto " << output_var_name << " = ";
        setupFileStream << function->name << "(" << getUntypedArgumentNames(*function) << ");" << std::endl;
        setupFileStream << jsonOutputGenerator->getJsonOutputText(output_var_name, function->retType);

        setupFileStream << getFreeVectorFreeText();
        setupFileStream << "} " << std::endl;

        setupFileStream << getMainText();
        std::string setupFileString = setupFileStream.str();

        std::ofstream of(dest_file_path, std::ofstream::out | std::ofstream::trunc);
        of << setupFileString;
    }


    std::string FunctionCallerGenerator::getTextForUserDefinedTypes() {
        std::stringstream output;
        std::reverse(declarationManager->userDefinedTypes.begin(), declarationManager->userDefinedTypes.end());
        for (auto &custom_type : declarationManager->userDefinedTypes) {
            output << FunctionCallerGenerator::getTextForUserDefinedType(custom_type);
        }
        std::reverse(declarationManager->userDefinedTypes.begin(), declarationManager->userDefinedTypes.end());
        return output.str();
    }

    // assumes all dependencies are defined beforehand
    std::string FunctionCallerGenerator::getTextForUserDefinedType(Type *type) {
        std::stringstream output;
        output << "typedef struct " << type->getCTypeName() << " {" << std::endl;
        for (auto &member: type->getNamedMembers()) {
            if (member.getType()->isArrayTy()) {
                output << member.getType()->getArrayElementType()->getCTypeName() << " " << member.getName();
                output << "[" << member.getType()->getArrayNumElements() << "]";
            } else {
                output << member.getType()->getCTypeName() << " " << member.getName();
            }
            output << ";" << std::endl;
        }
        output << "} " << type->getCTypeName() << ";" << std::endl;
        return output.str();
    }


    std::string FunctionCallerGenerator::getUntypedArgumentNames(Function &f) {
        std::stringstream output;
        for (auto &args : f.arguments) {
            output << args.getName() << ",";
        }
        auto retstring = output.str();
        if (!retstring.empty())
            retstring.pop_back(); //remove last ,

        return retstring;
    }


    std::string FunctionCallerGenerator::getGlobalDeclarationsText() {
        std::stringstream s;
        for (auto &global : this->declarationManager->globals) {
            s << global.getNameWithType() << ";" << std::endl;
        }
        return s.str();
    }



    void FunctionCallerGenerator::addMetaDataForPointerToJson(nlohmann::json &json, Type &arg) {
        if(!arg.isPointerTy())
            throw std::invalid_argument("needs to be a pointer");

        addMetaDataForTypeToJson(json, *arg.getPointerBaseElementType(), arg.getPointerDepth());
    }

    void FunctionCallerGenerator::addMetaDataForTypeToJson(nlohmann::json &json, Type &arg) {
        addMetaDataForTypeToJson(json, arg, 0);
    }

    void FunctionCallerGenerator::addMetaDataForTypeToJson(nlohmann::json &json, Type &arg, int pointerIndirections) {
        json["meta_data"]["pointer_depth"] = std::to_string(pointerIndirections);
        if(arg.isPointerTy()){
            addMetaDataForPointerToJson(json, arg);
            return;
        }
        if(arg.isScalarTy())
            json["meta_data"]["type"] = arg.getCTypeName();
        else
            json["meta_data"]["type"] = arg.getTypeName();

        if (arg.isArrayTy()) {
            json["meta_data"]["element_type"] = arg.getArrayElementType()->getCTypeName();
            json["meta_data"]["size"] = arg.getArrayNumElements();
        }

        if (arg.isStructTy()) {
            json["meta_data"]["name"] = arg.getCTypeName();
            json["meta_data"]["is_self_referential"] = arg.isCyclicWithItself;
        }
    }


    nlohmann::json FunctionCallerGenerator::getJsonInputTemplateTextForJsonRvalueAsJson(Type &arg, std::vector<std::pair<Type *, std::string>> typePath, int pointerIndirections) {
        if (arg.isPointerTy())
            return getJsonInputTemplateTextForJsonRvalueAsJson(*arg.getPointerElementType(), typePath, pointerIndirections + 1);

        nlohmann::json json;
        addMetaDataForTypeToJson(json, arg, pointerIndirections);

        if (arg.isScalarTy()) {
            json["value"] = nullptr;
        }
        if (arg.isArrayTy()) {
            json["meta_data"]["element_type"] = arg.getArrayElementType()->getCTypeName();
            json["meta_data"]["size"] = arg.getArrayNumElements();

            nlohmann::json arrayJson;
            for (int i = 0; i < arg.getArrayNumElements(); i++) {
                arrayJson.emplace_back(getJsonInputTemplateTextForJsonRvalueAsJson(*arg.getArrayElementType(), typePath));
            }
            json["value"] = arrayJson;
        }

        if (arg.isStructTy()) {
            for (auto &mem : arg.getNamedMembers()) {
                std::vector<std::pair<Type *, std::string>>::iterator pathToPreviousUnrollingOfType;
                pathToPreviousUnrollingOfType = std::find_if(typePath.begin(), typePath.end(),
                                                             [mem](const std::pair<Type *, std::string> &pair) { return mem.type == pair.first; });
                if (pathToPreviousUnrollingOfType != typePath.end()) {
                    addMetaDataForTypeToJson(json["value"][mem.getName()], *mem.getType());
                    json["value"][mem.getName()]["value"] = nullptr;
                } else {
                    auto memberTypePath(typePath);
                    memberTypePath.emplace_back(mem.getType(), mem.getName());
                    json["value"][mem.getName()] = getJsonInputTemplateTextForJsonRvalueAsJson(*mem.getType(), memberTypePath);
                }
            }
        }

        return json;
    }

    void FunctionCallerGenerator::generate_json_input_template_file(const std::string &dest_file_path) {
        std::ofstream of(dest_file_path, std::ofstream::out | std::ofstream::trunc);

        std::stringstream output;

        nlohmann::json json;

        for (auto &global: declarationManager->globals) {
            nlohmann::json jsonForGlobal;
            std::vector<std::pair<Type *, std::string >> typePath;
            typePath.emplace_back(global.getType(), global.getName());
            json["globals"][global.getName()] = getJsonInputTemplateTextForJsonRvalueAsJson(*global.getType(), typePath);
        }

        for (auto &arg: function->arguments) {
            nlohmann::json jsonForArg;
            std::vector<std::pair<Type *, std::string >> typePath;
            typePath.emplace_back(arg.getType(), arg.getName());
            if(arg.getType()->isPointerTy() && arg.getType()->getPointerBaseElementType()->isArrayTy()){
                std::vector<nlohmann::json> rvalues;
                rvalues.push_back(getJsonInputTemplateTextForJsonRvalueAsJson(*arg.getType(), typePath));
                json["arguments"][arg.getName()]["value"] = rvalues;
            }
            else
                json["arguments"][arg.getName()] = getJsonInputTemplateTextForJsonRvalueAsJson(*arg.getType(), typePath);

        }
        of << json.dump(4) << std::endl;
    }


    std::string FunctionCallerGenerator::getMainText() {
        return R"(
        int main(int argc, char *argv[]) {
            setupParser();
            parser.add_argument("input_file").help("The json file containing arguments");
            try {
                parser.parse_args(argc, argv);
            }
            catch (const std::runtime_error& err) {
                std::cout << err.what() << std::endl;
                std::cout << parser;
                exit(0);
            }
            callFunction();
            return 0;
        }
        )";
    }

    std::string FunctionCallerGenerator::getFreeVectorFreeText() {
        std::stringstream output;
        output << "for(auto& p : " << this->declarationManager->getFreeVectorName() << ") " << std::endl << "free(p);";
        return output.str();
    }

    std::vector<GlobalVariable> FunctionCallerGenerator::getGlobals() {
        return declarationManager->globals;
    }
}

