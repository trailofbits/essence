#pragma once
#include <sstream>
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
        std::string getTypeName();

        Type(TYPE_NAMES typeName) : type(typeName){};
        Type(TYPE_NAMES typeName, unsigned int const intSize): type(typeName), integerSize(intSize){};
        Type(TYPE_NAMES typeName, Type* pointerElementType): type(typeName), pointerElementType(pointerElementType){};
        Type(TYPE_NAMES typeName, Type* arrayElementType, uint64_t arraySize):type(typeName), arrayElementType(arrayElementType), arraySize(arraySize){};
        Type(TYPE_NAMES typeName, std::string structName, bool isUnion = false)
                : type(typeName), structName(structName), structIsUnion(isUnion){}



        // scalar values
        bool isVoidTy() { return type == TYPE_NAMES::VOID;};
        bool isIntegerTy() { return type == TYPE_NAMES::INTEGER;};
        bool isIntegerTy(int size) { return type == TYPE_NAMES::INTEGER && integerSize == size;};
        int getBitWidth(){ return integerSize;};
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
        void setMembers(std::vector<NamedVariable> members) {
            structMembers = members;
        }

        std::vector<NamedVariable> getNamedMembers() { return structMembers;};
        /*
         * Denotes whether if following all members of the structure would eventually lead down to the same type
         */
        bool isCyclicWithItself;


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
        NamedVariable(std::string name, Type* type): name(name), type(type){};
        Type* type;
        std::string name;
        Type* getType() { return this->type;};
        std::string getName() { return this->name;};
    };




    struct GlobalVariable : NamedVariable {
        GlobalVariable(std::string name, Type* type) : NamedVariable(name, type){};
    };

    struct Argument : NamedVariable {
    public:
        Argument(std::string name, Type* type, bool isPassByValue, bool isSRet) : NamedVariable(name, type), isPasByValue(isPassByValue), isSRet(isSRet){};
        bool isSRet;
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
        std::string getFunctionSignature();

        std::string getPurityName();

    private:
        std::string getTypedArgumentNames();
    };
}
