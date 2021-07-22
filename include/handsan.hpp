//
// Created by sabastiaan on 19-07-21.
//

#ifndef HANDSANITIZER_HANDSAN_HPP
#define HANDSANITIZER_HANDSAN_HPP

#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"

namespace handsanitizer {
    enum class TYPE_NAMES{
        VOID,
        INTEGER, // bool is an integer
        FLOAT,
        DOUBLE,
        STRUCT,
        ARRAY,
        POINTER
    };
    struct NamedVariable;

    class Type {
    public:
        std::string getCTypeName();

        Type(TYPE_NAMES typeName) : type(typeName){};
        Type(TYPE_NAMES typeName, unsigned int const intSize): type(typeName), integerSize(intSize){};
        Type(TYPE_NAMES typeName, Type* pointerElementType): type(typeName), pointerElementType(pointerElementType){};
        Type(TYPE_NAMES typeName, Type* arrayElementType, uint64_t arraySize):type(typeName), arrayElementType(arrayElementType), arraySize(arraySize){};
        Type(TYPE_NAMES typeName, std::string structName, std::vector<NamedVariable> members, bool isUnion = false): type(typeName), structName(structName), structMembers(members), structIsUnion(isUnion){};


        // scalar values
        bool isVoidTy() { return type == TYPE_NAMES::VOID;};
        bool isIntegerTy(int size = 32) { return type == TYPE_NAMES::INTEGER && integerSize == size;};
        bool isFloatTy() { return type == TYPE_NAMES::FLOAT;};
        bool isDoubleTy() { return type == TYPE_NAMES::DOUBLE;};

        // array
        bool isArrayTy() { return type == TYPE_NAMES::ARRAY;};
        int getArrayNumElements() { return arraySize;};
        Type *getArrayElementType() { return arrayElementType;};

        // pointer
        bool isPointerTy() { return type == TYPE_NAMES::POINTER;};
        Type *getPointerElementType() { return pointerElementType;};

        // struct
        bool isStructTy() { return type == TYPE_NAMES::STRUCT;};
        bool isUnion() { return structIsUnion;};
        std::vector<NamedVariable> getNamedMembers() { return structMembers;};

    private:
        TYPE_NAMES type;
        unsigned int integerSize = 0;
        Type* pointerElementType = nullptr;
        Type* arrayElementType = nullptr;;
        uint64_t arraySize = 0;
        std::string structName = "";
        std::vector<NamedVariable> structMembers;
        bool structIsUnion = false;
    };



    struct NamedVariable{
        NamedVariable(std::string name, Type* type){
            this->name = name;
            this->type = type;
            this->passByVal = passByVal;
        };
    public:
        Type *type;
        std::string name;
        bool passByVal;
        Type* getType() { return this->type;};
        std::string getName() { return this->name;};
    };


    struct GlobalVariable : NamedVariable {
        GlobalVariable(std::string name, Type* type) : NamedVariable(name, type){};
    };

    struct Argument : NamedVariable {
    public:
        Argument(std::string name, Type* type, bool isPassByValue) : NamedVariable(name, type), isPasByValue(isPassByValue){};
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

        std::string name;
        Type *retType;
        std::vector<Argument> arguments; // implicitly ordered
        Purity purity;
    };
}

#endif //HANDSANITIZER_HANDSAN_HPP

