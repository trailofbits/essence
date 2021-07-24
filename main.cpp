//
// Created by sabastiaan on 22-06-21.
//
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"


#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/WithColor.h"
#include <system_error>
#include <iostream>
#include <sstream>
#include <fstream>
#include "llvm/IR/LegacyPassManager.h"



#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

#include "llvm/Transforms/Utils.h"
#include <filesystem>

#include "include/name_generation.hpp"
#include "include/code_generation.hpp"
#include "include/handsan.hpp"
#include "LLVMExtractor.hpp"
#include <argparse/argparse.hpp>

static llvm::ExitOnError ExitOnErr;
//
//static llvm::cl::opt<std::string>
//        InputFilename(llvm::cl::Positional, llvm::cl::desc("<input bitcode>"), llvm::cl::init("-"));


int main(int argc, char** argv){

    argparse::ArgumentParser program("handsanitizer", "0.1.0");
    program.add_argument("bitcodeFile");
    program.add_argument("-o", "--output-directory").default_value(std::string("output")); // parameter packing
    program.add_argument("-g", "--generate-specifications").default_value(false).implicit_value(true); // parameter packing
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
//    llvm::cl::ParseCommandLineOptions(argc, argv, "llvm .bc -> .ll disassembler\n");

    std::string inputFilename = program.get("bitcodeFile");
    std::unique_ptr<llvm::MemoryBuffer> MB = ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFileOrSTDIN(inputFilename)));
    llvm::BitcodeFileContents IF = ExitOnErr(llvm::getBitcodeFileContents(*MB));

    std::string output_dir = program.get<std::string>("-o");
    if(!std::filesystem::exists(output_dir))
        std::filesystem::create_directory(output_dir);
    bool genSpecification = program.get<bool>("-g");


    handsanitizer::ModuleFromLLVMModuleFactory factory;

    for(auto& llvm_mod : IF.Mods){

        std::unique_ptr<llvm::Module> mod = ExitOnErr(llvm_mod.parseModule(Context));
        auto extractedMod = factory.ExtractModule(Context, mod);
        if(genSpecification){


            std::filesystem::path p(inputFilename);
            p.replace_extension("");
            auto newFileName = p.filename().string();
            auto outputPath = std::filesystem::path();
            outputPath.append(output_dir);
            outputPath.append(newFileName);
            outputPath.replace_extension("spec");
            auto outputModFilename = outputPath.string();

            extractedMod.generate_json_module_specification(outputModFilename);
        }
        else{
            auto functions = program.get<std::vector<std::string>>("functions");

            for(auto& f : extractedMod.functions){
                if(functions.size() > 0){
                    if(std::find(functions.begin(), functions.end(), f.name) == functions.end()){
                        continue; // if functions are specified we only output those, otherwise output all funcs
                    }
                }

                std::cout << "generating executable for function: " << f.name << std::endl;
                extractedMod.generate_cpp_file_for_function(f, output_dir + "/" + f.name + ".cpp");
                extractedMod.generate_json_input_template_file(f, output_dir + "/" + f.name + ".json");
            }
        }
    }
    return 0;
}