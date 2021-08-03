#pragma once

#include <llvm/Bitcode/BitcodeReader.h>
#include "../include/handsan.hpp"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Error.h"
#include "Module.h"

namespace handsanitizer{
    // should wrap this in a Module Factory

    class ModuleFromLLVMModuleFactory{
    public:
        // so extraction sets its self onto the module
        Module ExtractModule(llvm::LLVMContext& context, std::unique_ptr<llvm::Module> const& mod);
    private:
        std::vector<Function> ExtractFunctions(Module& mod, std::unique_ptr<llvm::Module> const& llvm_mod);
        std::vector<GlobalVariable> ExtractGlobalVariables(Module& mod,std::unique_ptr<llvm::Module> const& llvm_mod);
        std::vector<Argument> ExtractArguments(Module& mod, llvm::Function& llvm_mod);
        Type* ConvertLLVMTypeToHandsanitizerType(Module* module, llvm::Type* type);


        bool functionHasCABI(llvm::Function &f);
        std::vector<std::pair<Type*, llvm::Type*>> user_defined_types;

        bool hasStructDefined(Module& mod, llvm::Type* type);
        Type* getDefinedStructByLLVMType(Module& mod, llvm::Type* type);
        void defineStructIfNeeded(Module& mod, llvm::Type* type);

        Purity getPurityOfFunction(const llvm::Function &f);

        std::string getStructNameFromLLVMType(Module& mod, const llvm::Type *type) const;

        Type *
        getReturnType(const llvm::Function &f, std::vector<handsanitizer::Argument> &args_of_func, Module &mod);
    };
}
