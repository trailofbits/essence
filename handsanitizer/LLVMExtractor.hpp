#pragma once

#include <llvm/Bitcode/BitcodeReader.h>
#include "handsan.hpp"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Error.h"
#include "FunctionCallerGenerator.h"

namespace handsanitizer{
    // should wrap this in a FunctionCallerGenerator Factory

    class ModuleFromLLVMModuleFactory{
    public:
        // so extraction sets its self onto the module
        std::vector<FunctionCallerGenerator> ExtractAllFunctionCallerGenerators(llvm::LLVMContext& context, std::unique_ptr<llvm::Module> const& mod);
    private:
        std::unique_ptr<Function> ExtractFunction(std::shared_ptr<DeclarationManager> declarationManager, llvm::Function& function);

        void DeclareAllCustomTypesInDeclarationManager(std::shared_ptr<DeclarationManager> declarationManager, std::unique_ptr<llvm::Module> const& llvm_mod);
        void DeclareGlobalsInManager(std::shared_ptr<DeclarationManager> declarationManager, std::unique_ptr<llvm::Module> const& llvm_mod);
        std::vector<Argument> ExtractArguments(std::shared_ptr<DeclarationManager> declarationManager, llvm::Function& llvm_mod);
        Type* ConvertLLVMTypeToHandsanitizerType(std::shared_ptr<DeclarationManager> declarationManager, llvm::Type* type, std::vector<llvm::Type *> &previouslySeenTypes);


        bool functionHasCABI(llvm::Function &f);


        /*
         * We require to know both handsanitizer types and LLVM types together as it might be that the handsan types slightly differ
         * for instance the member names and struct names themselves might differ (for instance when the llvm type has no name)
         * and typically we recursively walk llvm types which is easier if we do it like this
         */
        std::vector<std::pair<Type*, llvm::Type*>> defined_llvm_types;

        bool hasStructDefined(std::shared_ptr<DeclarationManager> declarationManager, llvm::Type* type);
        Type* getDefinedStructByLLVMType(std::shared_ptr<DeclarationManager> declarationManager, llvm::Type* type);
        void defineStructIfNeeded(std::shared_ptr<DeclarationManager> declarationManager,
                                  llvm::Type *type,
                                  std::vector<llvm::Type *> &previouslySeenTypes);

        Purity getPurityOfFunction(const llvm::Function &f);

        std::string getStructNameFromLLVMType(std::shared_ptr<DeclarationManager> declarationManager, const llvm::Type *type) const;

        Type*
        getReturnType(const llvm::Function &f, std::vector<handsanitizer::Argument> &args_of_func, std::shared_ptr<DeclarationManager> declarationManager);

        bool shouldGlobalBeIncluded(const llvm::GlobalValue &global) const;
    };
}
