#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include <system_error>
#include <iostream>
#include <fstream>



#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include <filesystem>

#include "LLVMExtractor.hpp"
#include <argparse/argparse.hpp>
#include "SpecificationPrinter.h"

static llvm::ExitOnError ExitOnErr;


void throwIfSpecifiedFunctionsDoNotExistInModule(std::vector<handsanitizer::FunctionCallerGenerator> &extractedFunctions,
                                                 std::vector<std::string> &functions);

int main(int argc, char** argv){
    argparse::ArgumentParser program("handsanitizer", "0.1.0");
    program.add_argument("bitcodeFile");
    program.add_argument("--no-template").default_value(false).implicit_value(true); // parameter packing
    program.add_argument("-o", "--output-directory").default_value(std::string("output")); // parameter packing
    program.add_argument("-b", "--build").default_value(false).implicit_value(true); // should be implicit?
    program.add_argument("functions").remaining();

    try {
        program.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cout << err.what() << std::endl;
        std::cout << program;
        exit(0);
    }


    // parse llvm and open module
    llvm::InitLLVM X(argc, argv);
    ExitOnErr.setBanner(std::string(argv[0]) + ": error: ");
    llvm::LLVMContext Context;

    std::string inputFilename = program.get("bitcodeFile");
    std::unique_ptr<llvm::MemoryBuffer> MB = ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFileOrSTDIN(inputFilename)));
    llvm::BitcodeFileContents IF = ExitOnErr(llvm::getBitcodeFileContents(*MB));

    std::string output_dir = program.get<std::string>("-o");
    if(!std::filesystem::exists(output_dir))
        std::filesystem::create_directory(output_dir);

    bool buildFlag = program.get<bool>("-b");
    bool skipInputTemplate = program.get<bool>("--no-template");

    handsanitizer::ModuleFromLLVMModuleFactory factory;

    for(auto& llvm_mod : IF.Mods){
        std::unique_ptr<llvm::Module> mod = ExitOnErr(llvm_mod.parseModule(Context));

        auto extractedFunctions = factory.ExtractAllFunctionCallerGenerators(Context, mod);
        if(buildFlag == false){
            handsanitizer::SpecificationPrinter specPrinter(extractedFunctions);
            specPrinter.printSpecification(output_dir, inputFilename);
        }
        else{
            for(auto& f : extractedFunctions){
                auto functions = program.get<std::vector<std::string>>("functions");
                if(functions.size() > 0)
                    throwIfSpecifiedFunctionsDoNotExistInModule(extractedFunctions, functions);

                f.generate_cpp_file_for_function(output_dir + "/" + f.function->name + ".cpp");
                if(!skipInputTemplate)
                    f.generate_json_input_template_file(output_dir + "/" + f.function->name + ".json");
            }
        }
    }
    return 0;
}

void throwIfSpecifiedFunctionsDoNotExistInModule(std::vector<handsanitizer::FunctionCallerGenerator> &extractedFunctions,
                                                 std::vector<std::string> &functions) {
    for(auto& input_func_name: functions){
        if(std::find_if(extractedFunctions.begin(), extractedFunctions.end(),
                        [&input_func_name](handsanitizer::FunctionCallerGenerator& extracted_fcg){ return extracted_fcg.function->name == input_func_name;}) == extractedFunctions.end()){
            throw std::invalid_argument("module does not contain function: " + input_func_name);
        }
    }
}
