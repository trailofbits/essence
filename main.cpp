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


static llvm::ExitOnError ExitOnErr;

static llvm::cl::opt<std::string>
        InputFilename(llvm::cl::Positional, llvm::cl::desc("<input bitcode>"), llvm::cl::init("-"));


int main(int argc, char** argv){
    // parse llvm and open module
    llvm::InitLLVM X(argc, argv);
    ExitOnErr.setBanner(std::string(argv[0]) + ": error: ");
    llvm::LLVMContext Context;
    llvm::cl::ParseCommandLineOptions(argc, argv, "llvm .bc -> .ll disassembler\n");
    std::unique_ptr<llvm::MemoryBuffer> MB = ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFileOrSTDIN(InputFilename)));
    llvm::BitcodeFileContents IF = ExitOnErr(llvm::getBitcodeFileContents(*MB));
    std::cout << "Bitcode file contains this many modules " << IF.Mods.size() << std::endl;

    handsanitizer::ModuleFromLLVMModuleFactory factory;
    for(auto& llvm_mod : IF.Mods){
        std::unique_ptr<llvm::Module> mod = ExitOnErr(llvm_mod.parseModule(Context));
        auto extractedMod = factory.ExtractModule(Context, mod);


        std::cout << "Applying to mod " << std::endl;
        for(auto& f : extractedMod.functions){
            // TODO: use proper filesystem apis for filenames
            if(!std::filesystem::exists(OUTPUT_DIR))
                std::filesystem::create_directory(OUTPUT_DIR);


            std::cout << "generating for f" << std::endl;
            extractedMod.generate_cpp_file_for_function(f, OUTPUT_DIR + "/" + f.name + ".cpp");
            extractedMod.generate_json_input_template_file(f, OUTPUT_DIR + "/" + f.name + ".json");
            extractedMod.generate_json_module_specification(OUTPUT_DIR+ "/" + "mod.json");

        }
    }
}