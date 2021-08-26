#include <iostream>
#include <fstream>
#include "FunctionCallerGenerator.h"
#include <nlohmann/json.hpp>

namespace handsanitizer {
    void FunctionCallerGenerator::generate_cpp_file_for_function(const std::string& dest_file_path) {
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
        for (auto &custom_type : declarationManager->userDefinedTypes ) {
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
            s << "" << global.getType()->getCTypeName() << " " << global.getName() << ";" << std::endl;
        }
        return s.str();
    }

    /*
     * seen types is an ordered list
     * with the idea that if the current type is inside the list, we print the entire path
     */
    std::string getJsonInputTemplateTextForJsonRvalue(Type &arg, std::vector<std::pair<Type*, std::string>> typePath) {
        std::stringstream output;
        if (arg.isPointerTy())
            output << getJsonInputTemplateTextForJsonRvalue(*arg.getPointerElementType(), typePath); // we abstract pointers away

        // if scalar
        if (!arg.isArrayTy() && !arg.isStructTy() && !arg.isPointerTy()) {
            output << "\"" << arg.getCTypeName() << "\"";
        }

        if (arg.isArrayTy()) {
            output << "[";
            for (int i = 0; i < arg.getArrayNumElements(); i++) {
                output << getJsonInputTemplateTextForJsonRvalue(*arg.getArrayElementType(), typePath);
                if (i != arg.getArrayNumElements() - 1) {
                    output << ",";
                }
            }
            output << "]";
        }

        if (arg.isStructTy()) {
            output << "{";


            for (auto &mem : arg.getNamedMembers()) {
                std::vector<std::pair<Type *, std::string>>::iterator pathToPreviousUnrollingOfType;
                pathToPreviousUnrollingOfType = std::find_if(typePath.begin(), typePath.end(),
                                                             [mem](const std::pair<Type *, std::string>& pair) { return mem.type == pair.first; });
                if(pathToPreviousUnrollingOfType != typePath.end()){
                    // member type is already found
                    std::stringstream pathToFirstOccurrence;
                    for(auto& path : typePath)
                        pathToFirstOccurrence << "[\'" << path.second << "\']";

                    output << "\"" << mem.getName() << "\": \"cycles_with_" << pathToFirstOccurrence.str() << "\"";
                }
                else{
                    auto memberTypePath(typePath);
                    memberTypePath.emplace_back(mem.getType(), mem.getName());
                    output << "\"" << mem.getName() << "\"" << ": " << getJsonInputTemplateTextForJsonRvalue(*mem.getType(), memberTypePath);
                }

                if (mem.getName() != arg.getNamedMembers().back().getName()) { // names should be unique
                    output << "," << std::endl;
                }
            }
            output << "}";
        }
        return output.str();
    }

    void FunctionCallerGenerator::generate_json_input_template_file(const std::string& dest_file_path) {
        std::ofstream of(dest_file_path, std::ofstream::out | std::ofstream::trunc);

        std::stringstream output;

        output << "{" << std::endl;
        output << "\"globals\": {" << std::endl;
        for (auto &arg: this->declarationManager->globals) {
            std::vector<std::pair<Type*, std::string >> typePath;
            typePath.emplace_back(arg.getType(), arg.getName());

            output << "\"" << arg.getName() << "\"" << ": " << getJsonInputTemplateTextForJsonRvalue(*arg.getType(), typePath);
            if (arg.getName() != this->declarationManager->globals.back().getName()) {
                output << "," << std::endl;
            }
        }

        output << "}," << std::endl;

        output << "\"arguments\": {" << std::endl;
        for (auto &arg: function->arguments) {
            std::vector<std::pair<Type*, std::string >> typePath;
            typePath.emplace_back(arg.getType(), arg.getName());
            output << "\"" << arg.getName() << "\"" << ": " << getJsonInputTemplateTextForJsonRvalue(*arg.getType(), typePath);
            if (arg.getName() != function->arguments.back().getName()) {
                output << "," << std::endl;
            }
        }
        output << "}" << std::endl;
        output << "}" << std::endl;

        auto j = nlohmann::json::parse(output.str());
        of << j.dump(4) << std::endl;
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

