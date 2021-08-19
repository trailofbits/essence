#include <iostream>
#include "LLVMExtractor.hpp"
#include "handsan.hpp"
#include "FunctionCallerGenerator.h"


static llvm::ExitOnError ExitOnErr;

std::string getNameForStructMember(int index) {
    return "a" + std::to_string(index);
}

namespace handsanitizer {
    std::vector<FunctionCallerGenerator>
    ModuleFromLLVMModuleFactory::ExtractAllFunctionCallerGenerators(llvm::LLVMContext &context, std::unique_ptr<llvm::Module> const &mod) {
        std::vector<FunctionCallerGenerator> fcgs;
        for(auto& function : mod->functions()){
            this->defined_llvm_types.clear();

            if(!functionHasCABI(function))
                continue;

            std::shared_ptr<DeclarationManager> declarationManager = std::make_shared<DeclarationManager>();
            DeclareAllCustomTypesInDeclarationManager(declarationManager, mod);
            DeclareGlobalsInManager(declarationManager, mod);
            std::unique_ptr<Function> extractedFunc = ExtractFunction(declarationManager, function);

            // make sure no names can be generated that are also used as function names
            for(auto& f : mod->functions())
                if(f.hasName())
                    declarationManager->addDeclaration(f.getName().str());

            fcgs.emplace_back(std::move(extractedFunc), declarationManager);
        }

        return fcgs;
    }

    void ModuleFromLLVMModuleFactory::defineStructIfNeeded(std::shared_ptr<DeclarationManager> declarationManager,
                                                           llvm::Type *type,
                                                           std::vector<llvm::Type *> &previouslySeenTypes) {
        if(type->isPointerTy())
            defineStructIfNeeded(declarationManager, type->getPointerElementType(), previouslySeenTypes);

        std::string type_str;
        llvm::raw_string_ostream rso(type_str);
        type->print(rso);

        if(!type->isStructTy())
            return;

        if (this->hasStructDefined(declarationManager, type)){
            auto r = std::find_if(defined_llvm_types.begin(), defined_llvm_types.end(), [&type](std::pair<Type*, llvm::Type*> defined_type){ return type == defined_type.second;});
            if(r != defined_llvm_types.end()) // llvm type was already declared and hence must be cyclic
                r->first->isCyclicWithItself = true;
            return;
        }

        bool isUnion = false;
        if (type->getStructName().find("union.") != std::string::npos)
            isUnion = true;

        std::string structName = getStructNameFromLLVMType(declarationManager, type);
        Type *newHandsanType = new Type(TYPE_NAMES::STRUCT, structName, isUnion);
        this->defined_llvm_types.push_back(std::pair<Type *, llvm::Type *>(newHandsanType, type));
        previouslySeenTypes.push_back(type);
        std::vector<NamedVariable> members;
        for (int i = 0; i < type->getStructNumElements(); i++) {
            llvm::Type *childType = type->getStructElementType(i);
            auto childName = getNameForStructMember(i);
            auto childMemType = ConvertLLVMTypeToHandsanitizerType(declarationManager, childType, previouslySeenTypes);
            members.push_back(NamedVariable(childName, childMemType));
        }
        newHandsanType->setMembers(members);
    }

    std::string ModuleFromLLVMModuleFactory::getStructNameFromLLVMType(
            std::shared_ptr<DeclarationManager> declarationManager, const llvm::Type *type) const {
        std::string structName = type->getStructName().str();
        if(structName == "")
            structName = declarationManager->getUniqueTmpCPPVariableNameFor("anon_struct");
        else if (structName.find("struct.") != std::string::npos)
            structName = structName.replace(0, 7, ""); //removes struct. prefix

        else if (structName.find("union.") != std::string::npos)
            structName = structName.replace(0, 6, ""); //removes union. prefix

        return structName;
    }

    std::unique_ptr<Function>
    ModuleFromLLVMModuleFactory::ExtractFunction(std::shared_ptr<DeclarationManager> declarationManager, llvm::Function& function) {
        std::string fname;
        if (function.hasName())
            fname = function.getName();
        else
            fname = declarationManager->getUniqueTmpCPPVariableNameFor();

        std::vector<handsanitizer::Argument> args_of_func = ExtractArguments(declarationManager, function);
        Type *retType = getReturnType(function, args_of_func, declarationManager);

        // TODO instead of hacking the removal of an sret we should fix generation to consider sret args
        args_of_func.erase(std::remove_if( args_of_func.begin(), args_of_func.end(), [](const Argument& arg) {
                    return arg.isSRet;
                }), args_of_func.end());
        Purity p = this->getPurityOfFunction(function);

        return std::make_unique<Function>(fname, retType, args_of_func, p);
    }

    Type *
    ModuleFromLLVMModuleFactory::getReturnType(const llvm::Function &f,
                                               std::vector<handsanitizer::Argument> &args_of_func,
                                               std::shared_ptr<DeclarationManager> declarationManager) {
        Type* retType;
        bool sret = false;
        for(auto& arg : args_of_func)
            if(arg.isSRet){
                retType = arg.getType();
                sret = true;
            }
        if(!sret){
            auto prevSeenTypes = std::vector<llvm::Type*>();
            retType = ConvertLLVMTypeToHandsanitizerType(declarationManager, f.getReturnType(), prevSeenTypes);
        }
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


    void ModuleFromLLVMModuleFactory::DeclareGlobalsInManager(std::shared_ptr<DeclarationManager> declarationManager,
                                                              std::unique_ptr<llvm::Module> const &llvm_mod) {
        std::vector<handsanitizer::GlobalVariable> globals;
        for (auto &global : llvm_mod->global_values()) {
            if (shouldGlobalBeIncluded(global)) {
                auto prevSeenTypes = std::vector<llvm::Type*>();
                auto globalType = this->ConvertLLVMTypeToHandsanitizerType(declarationManager, global.getType()->getPointerElementType(), prevSeenTypes);
                GlobalVariable globalVariable(global.getName().str(), globalType);
                declarationManager->addDeclaration(globalVariable);
            }
        }
    }

    bool ModuleFromLLVMModuleFactory::shouldGlobalBeIncluded(const llvm::GlobalValue &global) const {
        return !global.getType()->isFunctionTy() &&
                       !(global.getType()->isPointerTy() && global.getType()->getPointerElementType()->isFunctionTy()) &&
                       !global.isPrivateLinkage(global.getLinkage()) &&
                       !global.getType()->isMetadataTy() &&
                       global.isDSOLocal();
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

    Type* ModuleFromLLVMModuleFactory::ConvertLLVMTypeToHandsanitizerType(std::shared_ptr<DeclarationManager> declarationManager, llvm::Type *type, std::vector<llvm::Type *> &previouslySeenTypes) {
        if (type->isVoidTy())
            return new Type(TYPE_NAMES::VOID);

        else if (type->isIntegerTy())
            return new Type(TYPE_NAMES::INTEGER, type->getIntegerBitWidth());

        else if (type->isFloatTy())
            return new Type(TYPE_NAMES::FLOAT);

        else if (type->isDoubleTy())
            return new Type(TYPE_NAMES::DOUBLE);

        else if (type->isArrayTy())
            return new Type(TYPE_NAMES::ARRAY,
                               ConvertLLVMTypeToHandsanitizerType(declarationManager, type->getArrayElementType(), previouslySeenTypes),
                               type->getArrayNumElements());

        else if (type->isPointerTy())
            return new Type(TYPE_NAMES::POINTER,
                               ConvertLLVMTypeToHandsanitizerType(declarationManager, type->getPointerElementType(), previouslySeenTypes));

        else if (type->isStructTy()) {
            if (!this->hasStructDefined(declarationManager, type))
                this->defineStructIfNeeded(declarationManager, type, previouslySeenTypes);
            return this->getDefinedStructByLLVMType(declarationManager, type);
        }

        else if(type->isFunctionTy()){
            throw std::invalid_argument("Function type found, we do not support this");
        }

        else{

            std::string type_str;
            llvm::raw_string_ostream rso(type_str);
            type->print(rso);


            throw std::invalid_argument("Could not convert llvm type" + rso.str());
        }
    }

    bool ModuleFromLLVMModuleFactory::functionHasCABI(llvm::Function &f) {
        std::string funcName = f.getName().str();
        if (funcName.rfind("_Z", 0) != std::string::npos) {
            return false; // mangeld c++ function
        }
        return true;
    }

    bool ModuleFromLLVMModuleFactory::hasStructDefined(std::shared_ptr<DeclarationManager> declarationManager, llvm::Type *type) {
        for (auto &user_type : this->defined_llvm_types) {
            if (user_type.second == type)
                return true;
        }
        return false;
    }

    Type* ModuleFromLLVMModuleFactory::getDefinedStructByLLVMType(std::shared_ptr<DeclarationManager> declarationManager, llvm::Type *type) {
        for (auto &user_type : this->defined_llvm_types) {
            if (user_type.second == type)
                return user_type.first;
        }
        throw std::invalid_argument("Type is not declared yet");
    }

    std::vector<Argument> ModuleFromLLVMModuleFactory::ExtractArguments(
            std::shared_ptr<DeclarationManager> declarationManager, llvm::Function &llvm_mod) {
        std::vector<Argument> args;

        for (auto &arg : llvm_mod.args()) {
            if(arg.getType()->isMetadataTy())
                continue;

            std::string argName = "";
            if (arg.hasName())
                argName = arg.getName();
            else
                argName = declarationManager->getUniqueTmpCPPVariableNameFor();

            Type* argType;
            auto prevSeenTypes = std::vector<llvm::Type*>();
            if(arg.hasByValAttr() && arg.getType()->isPointerTy())
                argType = ConvertLLVMTypeToHandsanitizerType(declarationManager, arg.getType()->getPointerElementType(), prevSeenTypes);
            else
                argType = ConvertLLVMTypeToHandsanitizerType(declarationManager, arg.getType(), prevSeenTypes);

            args.push_back(Argument(argName, argType, arg.hasByValAttr(), arg.hasStructRetAttr()));
        }
        return args;
    }

    void ModuleFromLLVMModuleFactory::DeclareAllCustomTypesInDeclarationManager(std::shared_ptr<DeclarationManager> declarationManager,
                                                                                const std::unique_ptr<llvm::Module> &llvm_mod) {
        /*
         * During type creation it is useful to have both the llvm type as well as our internal type representation
         * Therefore we extract everything first to a local vector containing this mapping and only then declare them
         * inside the declaration manager
         */
        for(auto& global : llvm_mod->globals()){
            if(shouldGlobalBeIncluded(global)){
                auto prevSeenTypes = std::vector<llvm::Type*>();
                ConvertLLVMTypeToHandsanitizerType(declarationManager, global.getType()->getElementType(), prevSeenTypes); // all globals are pointers
            }
        }

        for(auto& function: llvm_mod->functions()){
            auto prevSeenTypes = std::vector<llvm::Type*>();
            ConvertLLVMTypeToHandsanitizerType(declarationManager, function.getReturnType(), prevSeenTypes);

            for(auto& arg_of_func : function.args()){
                prevSeenTypes.clear();
                if(!arg_of_func.getType()->isLabelTy() || !arg_of_func.getType()->isMetadataTy()){
                    ConvertLLVMTypeToHandsanitizerType(declarationManager, arg_of_func.getType(), prevSeenTypes);
                }
            }
        }

        for(auto& udt : this->defined_llvm_types){
            declarationManager->addDeclaration(udt.first);
        }
    }


    std::string Type::getCTypeName() {
        if (this->isPointerTy())
            return this->getPointerElementType()->getCTypeName() + "*";

        else if (this->integerSize == 1)
            return "bool";

        else if (this->integerSize == 8)
            return "char";

        else if (this->integerSize == 16)
            return "int16_t";

        else if (this->integerSize == 32)
            return "int32_t";

        else if (this->integerSize == 64)
            return "int64_t";

        else if (this->isVoidTy())
            return "void";

        else if (this->isFloatTy())
            return "float";

        else if (this->isDoubleTy())
            return "double";

        else if (this->isStructTy()) {
            return this->structName;
        }

        else if (this->isArrayTy())
            throw std::invalid_argument("Arrays can't have their names expressed like this");

        else {
            throw std::invalid_argument("Type: is not supported");
        }
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
