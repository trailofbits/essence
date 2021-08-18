//
// Created by sabastiaan on 22-07-21.
//
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include "FunctionCallerGenerator.h"

namespace handsanitizer {
    void FunctionCallerGenerator::generate_cpp_file_for_function(std::string dest_file_path) {
        declarationManager->clearGeneratedNames();
        std::stringstream setupfilestream;

        setupfilestream << "#include <cstdint>" << std::endl;
        setupfilestream << "#include <iomanip>" << std::endl;
        setupfilestream << "#include <argparse/argparse.hpp>" << std::endl;
        setupfilestream << "#include <fstream>" << std::endl;
        setupfilestream << "#include <nlohmann/json.hpp> // header only lib" << std::endl << std::endl;


        // DECLARE STRUCTS
        setupfilestream << getTextForUserDefinedTypes();

        // DECLARATION TEST FUNCTION
        // the LLVM type name differs from how we define it in our generated source code
        std::string functionSignature = function->getFunctionSignature();
        setupfilestream << "extern \"C\" " + functionSignature << std::endl;

        // DECLARE GLOBALS

        setupfilestream << "argparse::ArgumentParser parser;" << std::endl << std::endl;
        setupfilestream << "// globals from module" << std::endl << getGlobalDeclarationsText() << std::endl
                        << std::endl;


        // DEFINE SETUPPARSER
        setupfilestream << "void setupParser() { " << std::endl
                        << "std::stringstream s;"
                        << "s << \"Test program for: " << functionSignature << "\" << std::endl << R\"\"\"("
                        << getTextForUserDefinedTypes() << ")\"\"\";" << std::endl
                        << "\tparser = argparse::ArgumentParser(s.str());" << std::endl
                        << "} " << std::endl << std::endl;

        // DEFINE CALLFUNCTION
        setupfilestream << "void callFunction() { " << std::endl;
        setupfilestream << "std::vector<void*> " << declarationManager->getFreeVectorName() << ";" << std::endl
                        << "\t\t" << "std::string inputfile = parser.get<std::string>(\"input_file\");" << std::endl;
        auto inputFileName = declarationManager->getUniqueTmpCPPVariableNameFor("inputFile");
        auto jsonInputVariable = declarationManager->getUniqueTmpCPPVariableNameFor("j");
        setupfilestream << "\t\t" << "std::ifstream " << inputFileName << "(inputfile);\n"
                                                                          "\t\tnlohmann::json " << jsonInputVariable
                        << ";" << std::endl
                        << inputFileName
                        << " >> "
                        << jsonInputVariable << ";" << std::endl

                        << jsonInputParser->getParserRetrievalTextForGlobals(jsonInputVariable)
                        << jsonInputParser->getParserRetrievalTextForArguments(jsonInputVariable, function->arguments);

        auto output_var_name = declarationManager->getUniqueTmpCPPVariableNameFor("output");
        if (function->retType->isVoidTy() == false)
            setupfilestream << "auto " << output_var_name << " = ";
        setupfilestream << function->name << "(" << getUntypedArgumentNames(*function) << ");" << std::endl;
        setupfilestream << jsonOutputGenerator->getJsonOutputText(output_var_name, function->retType);

        setupfilestream << getFreeVectorFreeText();
        setupfilestream << "} " << std::endl;

        setupfilestream << getMainText();
        std::string setupFileString = setupfilestream.str();

        std::ofstream of;
        of.open(dest_file_path, std::ofstream::out | std::ofstream::trunc);
        of << setupFileString;
        of.close();
    }


    std::string FunctionCallerGenerator::getTextForUserDefinedTypes() {
        std::stringstream output;
        std::reverse(declarationManager->user_defined_types.begin(), declarationManager->user_defined_types.end());
        for (auto &custom_type : declarationManager->user_defined_types ) {
            output << FunctionCallerGenerator::getTextForUserDefinedType(custom_type);
        }
        std::reverse(declarationManager->user_defined_types.begin(), declarationManager->user_defined_types.end());
        return output.str();
    }

    // assumes all dependencies are defined before hand
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


    std::string getJsonInputTemplateTextForJsonRvalue(Type &arg) {
        std::stringstream output;
        if (arg.isPointerTy())
            output << getJsonInputTemplateTextForJsonRvalue(*arg.getPointerElementType()); // we abstract pointers away

        // if scalar
        if (arg.isArrayTy() == false && arg.isStructTy() == false && !arg.isPointerTy()) {
            output << arg.getCTypeName();
        }

        if (arg.isArrayTy()) {
            output << "[";
            for (int i = 0; i < arg.getArrayNumElements(); i++) {
                output << getJsonInputTemplateTextForJsonRvalue(*arg.getArrayElementType());
                if (i != arg.getArrayNumElements() - 1) {
                    output << ",";
                }
            }
            output << "]";
        }

        if (arg.isStructTy()) {
            output << "{";
            for (auto &mem : arg.getNamedMembers()) {
                output << "\"" << mem.getName() << "\"" << ": "
                       << getJsonInputTemplateTextForJsonRvalue(*mem.getType());
                if (mem.getName() != arg.getNamedMembers().back().getName()) { // names should be unique
                    output << "," << std::endl;
                }
            }
            output << "}";
        }
        return output.str();
    }

    void FunctionCallerGenerator::generate_json_input_template_file(std::string dest_file_path) {
        std::ofstream of;
        of.open(dest_file_path, std::ofstream::out | std::ofstream::trunc);

        std::stringstream output;
        output << "{" << std::endl;
        output << "\"globals\": {" << std::endl;
        for (auto &arg: this->declarationManager->globals) {
            output << "\"" << arg.getName() << "\"" << ": " << getJsonInputTemplateTextForJsonRvalue(*arg.getType());
            if (arg.getName() != this->declarationManager->globals.back().getName()) {
                output << "," << std::endl;
            }
        }

        output << "}," << std::endl;

        output << "\"arguments\": {" << std::endl;
        for (auto &arg: function->arguments) {
            output << "\"" << arg.getName() << "\"" << ": " << getJsonInputTemplateTextForJsonRvalue(*arg.getType());
            if (arg.getName() != function->arguments.back().getName()) {
                output << "," << std::endl;
            }
        }
        output << "}" << std::endl;

        output << "}" << std::endl;
        of << std::setw(4) << output.str() << std::endl;
        of.close();
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

