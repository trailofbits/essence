//
// Created by sabastiaan on 19-07-21.
//

#ifndef HANDSANITIZER_HANDSAN_HPP
#define HANDSANITIZER_HANDSAN_HPP

#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"

namespace handsanitizer {


    class handarg {
    public:
        std::string name;
        int position;
        llvm::Type *type;
        bool passByVal;

        handarg(std::string name, int position, llvm::Type *type, bool passByVal = false) {
            this->name = name;
            this->position = position;
            this->type = type;
            this->passByVal = passByVal;
        };

        llvm::Type *getType();

        std::string getName();
    };


    enum class TYPE_NAMES{
        VOID,
        INTEGER, // bool is an integer
        FLOAT,
        DOUBLE,
        STRUCT,
        ARRAY,
        POINTER
    };

    class Type {
    public:
        std::string getCTypeName();

        Type(TYPE_NAMES typeName);
        Type(TYPE_NAMES typeName, unsigned int const intSize);
        Type(TYPE_NAMES typeName, Type* pointerElementType);
        Type(TYPE_NAMES typeName, Type* arrayElementType, uint64_t arraySize);
        Type(TYPE_NAMES typeName, std::string structName, std::vector<std::tuple<std::string, Type*>> members, bool isUnion = false);
        Type(TYPE_NAMES typeName, Type* pointerElementType);

        // scalar values
        bool isVoidTy();
        bool isIntegerTy(int size = 32);
        bool isFloat();
        bool isDouble();

        // array
        bool isArrayTy();
        int getArrayNumElements();
        Type *getArrayElementType();

        // pointer
        bool isPointerTy();
        Type *getPointerElementType();

        // struct
        bool isStructTy();
        bool isUnion();
        std::vector<std::tuple<std::string, Type*>> getNamedMembers();
    };



    struct NamedVariable{
        NamedVariable(std::string name, Type* type){
            this->name = name;
            this->type = type;
            this->passByVal = passByVal;
        };
        Type *type;
        std::string name;
        bool passByVal;
        Type* getType();
        std::string getName();
    };


    class GlobalVariable : NamedVariable {
    public:
        GlobalVariable(std::string name, Type* type) : NamedVariable(name, type){};
    };

    class Argument : NamedVariable {
    public:
        Argument(std::string name, Type* type, bool isPassByValue) : NamedVariable(name, type){
            this->isPasByValue = isPassByValue;
        };
        bool isPasByValue;
    };

    enum class Purity {
        IMPURE,
        READ_NONE,
        WRITE_ONLY
    };

    class Function {
    public:
        Function(const std::string &name, Type *retType, std::vector<Argument> arguments, Purity purity);
        void generate_cpp_file(std::string dest_file_path);
        void generate_json_input_template_file(std::string dest_file_path);

        std::string name;
        Type *retType;
        std::vector<Argument> arguments; // implicitly ordered
        Purity purity;
        
    private:
        std::string getSetupFileText(
                std::vector<handarg> &globals); // globals are not part of the function
    };

    class Module {
    public:
        std::vector<Type*> user_defined_types;
        std::vector<GlobalVariable> globals;
        std::vector<Function> functions;

        std::string getUniqueTmpNameFor(std::vector<std::string> prefixes);
        std::string getUniqueTmpNameFor();

        void parse();
    };


    class DefinedStruct {
    public:
        bool isUnion = false;
        llvm::StructType *structType;
        std::string definedCStructName;
        std::vector<std::string> member_names; // implicitly ordered, can I make an tuple out of this combined with the member?
        std::vector<std::pair<std::string, handsanitizer::Type *>> getNamedMembers();
    };
}

#endif //HANDSANITIZER_HANDSAN_HPP

