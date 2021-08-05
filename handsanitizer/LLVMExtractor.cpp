#include <sstream>
#include "LLVMExtractor.hpp"
#include "../include/handsan.hpp"
#include "../include/name_generation.hpp"
#include "../include/code_generation.hpp"
#include "Module.h"


static llvm::ExitOnError ExitOnErr;

std::string getNameForStructMember(int index) {
    return "a" + std::to_string(index);
}

namespace handsanitizer {
    Module
    ModuleFromLLVMModuleFactory::ExtractModule(llvm::LLVMContext &context, std::unique_ptr<llvm::Module> const &mod) {
        Module handsan_mod;

        //TODO set this to input name?
        if(mod->getName() != "")
            handsan_mod.name = mod->getName();
        else
            handsan_mod.name = "extracted_module";
        handsan_mod.globals = this->ExtractGlobalVariables(handsan_mod, mod);
        handsan_mod.functions = this->ExtractFunctions(handsan_mod, mod);
        std::vector<Type *> user_types;
        for (auto &t: this->user_defined_types)
            user_types.push_back(t.first);

        handsan_mod.user_defined_types = user_types; // type definitions are done implicitly during conversion
        return handsan_mod;
    }

    void ModuleFromLLVMModuleFactory::defineStructIfNeeded(Module &mod, llvm::Type *type) {
        if (this->hasStructDefined(mod, type))
            return;

        std::vector<NamedVariable> members;
        for (int i = 0; i < type->getStructNumElements(); i++) {
            llvm::Type *childType = type->getStructElementType(i);
            auto childName = getNameForStructMember(i);
            auto childMemType = ConvertLLVMTypeToHandsanitizerType(&mod, childType);
            members.push_back(NamedVariable(childName, childMemType));
        }

        bool isUnion = false;
        if (type->getStructName().find("union.") != std::string::npos)
            isUnion = true;

        std::string structName = getStructNameFromLLVMType(mod, type);

        //TODO add a case for when the struct has no name by it self as can happen for return types
        Type *t = new Type(TYPE_NAMES::STRUCT, structName, members, isUnion);
        this->user_defined_types.push_back(std::pair<Type *, llvm::Type *>(t, type));
    }

    std::string ModuleFromLLVMModuleFactory::getStructNameFromLLVMType(Module& mod, const llvm::Type *type) const {
        std::string structName = type->getStructName().str();
        if(structName == "")
            structName = mod.getUniqueTmpCPPVariableNameFor("anon_struct");
        else if (structName.find("struct.") != std::string::npos)
            structName = structName.replace(0, 7, ""); //removes struct. prefix

        else if (structName.find("union.") != std::string::npos)
            structName = structName.replace(0, 6, ""); //removes union. prefix

        return structName;
    }

    std::vector<Function>
    ModuleFromLLVMModuleFactory::ExtractFunctions(Module &mod, std::unique_ptr<llvm::Module> const &llvm_mod) {
        std::vector<Function> funcs;
        for (auto &f : llvm_mod->functions()) {
            //TODO incorperate a setting for this such that we can still test
//            if (!this->functionHasCABI(f))
//                continue;

            std::string fname;
            if (f.hasName())
                fname = f.getName();
            else
                fname = mod.getUniqueTmpCPPVariableNameFor();

            std::vector<handsanitizer::Argument> args_of_func = ExtractArguments(mod, f);
            Type *retType = getReturnType(f, args_of_func, mod);

            // TODO instead of hacking the removal of an sret we should fix generation to consider sret args
            args_of_func.erase(std::remove_if( args_of_func.begin(), args_of_func.end(), [](const Argument& arg) {
                        return arg.isSRet;
                    }), args_of_func.end());
            Purity p = this->getPurityOfFunction(f);


            Function extracted_f(fname, retType, args_of_func, p);
            funcs.push_back(extracted_f);
        }
        return funcs;
    }

    Type *
    ModuleFromLLVMModuleFactory::getReturnType(const llvm::Function &f,
                                               std::vector<handsanitizer::Argument> &args_of_func,
                                               Module &mod) {
        Type* retType;
        bool sret = false;
        for(auto& arg : args_of_func)
            if(arg.isSRet){
                retType = arg.getType();
                sret = true;
            }
        if(!sret)
          retType = ConvertLLVMTypeToHandsanitizerType(&mod, f.getReturnType());
        return retType;
    }

    Purity ModuleFromLLVMModuleFactory::getPurityOfFunction(const llvm::Function &f) {
        Purity p;
        if (f.doesNotAccessMemory())
            p = Purity::READ_NONE;

        else if (f.doesNotReadMemory())
            p = Purity::WRITE_ONLY;
        else
            p = Purity::IMPURE;
        return p;
    }


    std::vector<GlobalVariable> ModuleFromLLVMModuleFactory::ExtractGlobalVariables(Module &hand_mod,
                                                                                    std::unique_ptr<llvm::Module> const &llvm_mod) {
        std::vector<handsanitizer::GlobalVariable> globals;
        for (auto &global : llvm_mod->global_values()) {
            // TODO: Should figure out what checks actually should be in here
            if (!global.getType()->isFunctionTy() &&
                !(global.getType()->isPointerTy() && global.getType()->getPointerElementType()->isFunctionTy()) &&
                !global.isPrivateLinkage(global.getLinkage()) &&
                !global.getType()->isMetadataTy() &&
                global.isDSOLocal()) {

                // globals are always pointers in llvm
                auto globalType = this->ConvertLLVMTypeToHandsanitizerType(&hand_mod, global.getType()->getPointerElementType());
                GlobalVariable globalVariable(global.getName().str(), // TODO .str
                                              globalType);
                globals.push_back(globalVariable);
            }
        }
        return globals;
    }

    Function::Function(const std::string &name, Type *retType, std::vector<Argument> arguments, Purity purity)
            : name(name), retType(retType), arguments(arguments), purity(purity) {}

    std::string Function::getFunctionSignature() {
        return this->retType->getCTypeName() + " " + this->name + "(" + this->getTypedArgumentNames() + ");";
    }

    std::string Function::getTypedArgumentNames() {
        std::stringstream output;
        for(auto& args : this->arguments){
            output << args.getType()->getCTypeName() << " " << args.getName() << ", ";
        }
        auto retstring = output.str();
        if(!retstring.empty()){
            retstring.pop_back(); //remove last ,<space>
            retstring.pop_back();
        }
        return retstring;
    }

    std::string Function::getPurityName() {
        if(this->purity == Purity::IMPURE)
            return "impure";
        else if(this->purity == Purity::READ_NONE)
            return "read_none";
        else
            return "write_only";
    }

    Type *ModuleFromLLVMModuleFactory::ConvertLLVMTypeToHandsanitizerType(Module *module, llvm::Type *type) {
        Type *newType = nullptr;

        if (type->isVoidTy())
            newType = new Type(TYPE_NAMES::VOID);

        if (type->isIntegerTy())
            newType = new Type(TYPE_NAMES::INTEGER, type->getIntegerBitWidth());

        if (type->isFloatTy())
            newType = new Type(TYPE_NAMES::FLOAT);

        if (type->isDoubleTy())
            newType = new Type(TYPE_NAMES::DOUBLE);

        if (type->isArrayTy())
            newType = new Type(TYPE_NAMES::ARRAY,
                               ConvertLLVMTypeToHandsanitizerType(module, type->getArrayElementType()),
                               type->getArrayNumElements());

        if (type->isPointerTy())
            newType = new Type(TYPE_NAMES::POINTER,
                               ConvertLLVMTypeToHandsanitizerType(module, type->getPointerElementType()));

        if (type->isStructTy()) {
            if (!this->hasStructDefined(*module, type))
                this->defineStructIfNeeded(*module, type);
            newType = this->getDefinedStructByLLVMType(*module, type);
        }

        if (newType == nullptr) {
            std::string type_str;
            llvm::raw_string_ostream rso(type_str);
            type->print(rso);


            throw std::invalid_argument("Could not convert llvm type" + rso.str());
        }

        return newType;
    }

    bool ModuleFromLLVMModuleFactory::functionHasCABI(llvm::Function &f) {
        std::string funcName = f.getName().str();
        if (funcName.rfind("_Z", 0) != std::string::npos) {
            return false; // mangeld c++ function
        }
        return true;
    }

    bool ModuleFromLLVMModuleFactory::hasStructDefined(Module &mod, llvm::Type *type) {
        for (auto &user_type : this->user_defined_types) {
            if (user_type.second == type)
                return true;
        }
        return false;
    }

    Type *ModuleFromLLVMModuleFactory::getDefinedStructByLLVMType(Module &mod, llvm::Type *type) {
        for (auto &user_type : this->user_defined_types) {
            if (user_type.second == type)
                return user_type.first;
        }
        throw std::invalid_argument("Type is not declared yet");
    }

    std::vector<Argument> ModuleFromLLVMModuleFactory::ExtractArguments(Module& mod, llvm::Function &f) {
        std::vector<Argument> args;

        for (auto &arg : f.args()) {
            if(arg.getType()->isMetadataTy()) // TODO is it a problem that arguments can be meta data?
                continue;

            std::string argName = "";
            if (arg.hasName())
                argName = arg.getName();
            else
                argName = mod.getUniqueTmpCPPVariableNameFor();

            Type* argType;
            if(arg.hasByValAttr() && arg.getType()->isPointerTy())
                argType = ConvertLLVMTypeToHandsanitizerType(&mod, arg.getType()->getPointerElementType());
            else
                argType = ConvertLLVMTypeToHandsanitizerType(&mod, arg.getType());

            args.push_back(Argument(argName, argType, arg.hasByValAttr(), arg.hasStructRetAttr()));
        }
        return args;
    }


    std::string Type::getCTypeName() {
        if (this->isPointerTy())
            return this->getPointerElementType()->getCTypeName() + "*";

        if (this->integerSize == 1)
            return "bool";

        if (this->integerSize == 8)
            return "char";

        if (this->integerSize == 16)
            return "int16_t";

        if (this->integerSize == 32)
            return "int32_t";

        if (this->integerSize == 64)
            return "int64_t";

        if (this->isVoidTy())
            return "void";

        if (this->isFloatTy())
            return "float";

        if (this->isDoubleTy())
            return "double";

        if (this->isStructTy()) {
            return this->structName;
        }

        if (this->isArrayTy())
            throw std::invalid_argument("Arrays can't have their names expressed like this");

        return "Not supported";
    }

    std::string Type::getTypeName() {
        if(this->isVoidTy())
            return "void";
        else if(this->isPointerTy())
            return "pointer";

        else if(this->isIntegerTy())
            return "integer";

        else if(this->isFloatTy())
            return "float";

        else if(this->isDoubleTy())
            return "double";

        else if(this->isStructTy())
            return "struct";

        else if(this->isArrayTy())
            return "array";

        else
            return "not_supported";
    }
}
