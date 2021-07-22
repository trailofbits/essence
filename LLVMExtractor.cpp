#pragma once
#include "LLVMExtractor.hpp"

static llvm::ExitOnError ExitOnErr;

std::string getNameForStructMember(int index){
    return "a" + std::to_string(index);
}

namespace handsanitizer{
    void ModuleFromLLVMModuleFactory::defineStructIfNeeded(llvm::Type *type) {
        if(this->hasStructDefined(type))
            return;

        std::vector<std::tuple<std::string, Type*>> members;
        for(int i = 0; i < type->getStructNumElements(); i++){
            llvm::Type* childType = type->getStructElementType(i);
            auto childName = getNameForStructMember(i);
            auto childMemType = ConvertLLVMTypeToHandsanitizerType(this, childType);
            members.push_back(std::tuple<std::string, Type*>(childName, childMemType));
        }

        bool isUnion = false;
        if(type->getStructName().find("union.") != std::string::npos)
            isUnion = true;

        std::string structName = type->getStructName();
        if(structName.find("struct.") != std::string::npos)
            structName = structName.replace(0, 7, ""); //removes struct. prefix

        else if (structName.find("union.") != std::string::npos)
            structName = structName.replace(0, 6, ""); //removes union. prefix

        //TODO add a case for when the struct has no name by it self as can happen for return types
        Type* t = new Type(TYPE_NAMES::STRUCT, structName, members, isUnion);
        this->user_defined_types.push_back(t);
    }


    Module ModuleFromLLVMModuleFactory::ExtractModule(llvm::LLVMContext& context, std::unique_ptr<llvm::Module> const& mod){
        Module handsan_mod;


        handsan_mod.globals = ExtractGlobalVariables(mod);
        handsan_mod.functions = ExtractFunctions(mod);
        handsan_mod.user_defined_types = this->user_defined_types;

        return handsan_mod;
    }


    std::vector<Function> ExtractFunctions(std::unique_ptr<llvm::Module> const& mod);
    std::vector<Argument> ExtractArguments(llvm::Function& mod);

    // users should clean up
//    std::vector<Type*> ExtractTypeDefinitions(std::unique_ptr<llvm::Module> const& mod){
//
//    }


    Purity getPurityOfFunction(const llvm::Function &f);

    std::vector<Function> ModuleFromLLVMModuleFactory::ExtractFunctions(Module& mod, std::unique_ptr<llvm::Module> const& llvm_mod){
        std::vector<Function> funcs;
        for(auto& f : llvm_mod->functions()) {
            if(!this->functionHasCABI(f))
                continue;

            std::string fname = "";
            if(f.hasName())
                fname = f.getName();
            else
                fname = mod.getUniqueTmpNameFor();

            std::vector<handsanitizer::Argument> args_of_func = ExtractArguments(f);
            Type* retType = ConvertLLVMTypeToHandsanitizerType(&mod, f.getReturnType());
            Purity p = getPurityOfFunction(f);

            Function extracted_f(f.getName(), retType,  args_of_func, p)
        }
    }

    Purity getPurityOfFunction(const llvm::Function &f) {
        Purity p;
        if(f.doesNotAccessMemory())
            p = Purity::READ_NONE;

        else if(f.doesNotReadMemory())
            p = Purity::WRITE_ONLY;
        else
            p = Purity::IMPURE;
        return p;
    }


    std::vector<GlobalVariable> ExtractGlobalVariables(Module& hand_mod, std::unique_ptr<llvm::Module> const& llvm_mod){
        std::vector<handsanitizer::GlobalVariable> globals;
        for(auto& global : llvm_mod->global_values())
        {
            // TODO: Should figure out what checks actually should be in here
            if(!global.getType()->isFunctionTy()  && !(global.getType()->isPointerTy() && global.getType()->getPointerElementType()->isFunctionTy())  && global.isDSOLocal()){
                GlobalVariable globalVariable(global.getName(), ConvertLLVMTypeToHandsanitizerType(&hand_mod, global.getType()));
                globals.push_back(globalVariable);
            }
        }
        return globals;
    }

    Function::Function(const std::string &name, Type *retType, std::vector<Argument> arguments, Purity purity)
            : name(name), retType(retType), arguments(arguments), purity(purity) {}


    std::vector<Function>
    ModuleFromLLVMModuleFactory::ExtractFunctions(Module &mod, const std::unique_ptr<llvm::Module> &llvm_mod) {
        return std::vector<Function>();
    }

    std::vector<GlobalVariable>
    ModuleFromLLVMModuleFactory::ExtractGlobalVariables(Module &mod, const std::unique_ptr<llvm::Module> &llvm_mod) {
        return std::vector<GlobalVariable>();
    }

    Type *ModuleFromLLVMModuleFactory::ConvertLLVMTypeToHandsanitizerType(Module *module, llvm::Type *type) {
        Type* newType = nullptr;
        if(type->isVoidTy())
            newType = new Type(TYPE_NAMES::VOID);

        if(type->isIntegerTy())
            newType = new Type(TYPE_NAMES::INTEGER, type->getIntegerBitWidth());

        if(type->isFloatTy())
            newType = new Type(TYPE_NAMES::FLOAT);

        if(type->isDoubleTy())
            newType = new Type(TYPE_NAMES::DOUBLE);

        if(type->isArrayTy())
            newType = new Type(TYPE_NAMES::ARRAY, ConvertLLVMTypeToHandsanitizerType(module,type->getArrayElementType()), type->getArrayNumElements());

        if(type->isPointerTy())
            newType = new Type(TYPE_NAMES::POINTER, ConvertLLVMTypeToHandsanitizerType(module,type->getPointerElementType()));

        if(type->isStructTy()){
            if(!this->hasStructDefined(type))
                this->defineStructIfNeeded(type);
            newType = this->getDefinedStructByLLVMType(type);
        }

        if(newType == nullptr)
            throw std::invalid_argument("Could not convert llvm type");

        return newType;
    }

    bool ModuleFromLLVMModuleFactory::functionHasCABI(llvm::Function& f) {
        std::string funcName = f.getName().str();
        if(funcName.rfind("_Z",0) != std::string::npos){
            return false; // mangeld c++ function
        }
        return true;
    }
}
