#pragma once
#include <sstream>
#include <utility>
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
        std::string getTypeName() const;

        explicit Type(TYPE_NAMES typeName) : type(typeName){};
        Type(TYPE_NAMES typeName, unsigned int const intSize): type(typeName), integerSize(intSize){};
        Type(TYPE_NAMES typeName, Type* pointerElementType): type(typeName), pointerElementType(pointerElementType){};
        Type(TYPE_NAMES typeName, Type* arrayElementType, uint64_t arraySize):type(typeName), arrayElementType(arrayElementType), arraySize(arraySize){};
        Type(TYPE_NAMES typeName, std::string structName, bool isUnion = false)
                : type(typeName), structName(std::move(structName)), structIsUnion(isUnion){}



        // scalar values
        [[nodiscard]] bool isVoidTy() const { return type == TYPE_NAMES::VOID;} ;
        [[nodiscard]] bool isScalarTy() const { return isIntegerTy() || isFloatTy() || isDoubleTy();};
        [[nodiscard]] bool isIntegerTy() const { return type == TYPE_NAMES::INTEGER;};
        [[nodiscard]] bool isIntegerTy(int size) const { return type == TYPE_NAMES::INTEGER && integerSize == size;};
        [[nodiscard]] unsigned int getBitWidth() const { return integerSize;};
        [[nodiscard]] bool isFloatTy() const { return type == TYPE_NAMES::FLOAT;};
        [[nodiscard]] bool isDoubleTy() const { return type == TYPE_NAMES::DOUBLE;};

        // array
        [[nodiscard]] bool isArrayTy() const { return type == TYPE_NAMES::ARRAY;};
        [[nodiscard]] int getArrayNumElements() const { return arraySize;};
        [[nodiscard]] Type *getArrayElementType() const { return arrayElementType;};

        // pointer
        [[nodiscard]] bool isPointerTy() const { return type == TYPE_NAMES::POINTER;};
        [[nodiscard]] Type *getPointerElementType() const { return pointerElementType;};
        [[nodiscard]] Type *getPointerBaseElementType() const {
            Type* lastType = pointerElementType;
            while(lastType->isPointerTy()){
                lastType = lastType->getPointerElementType();
            }
            return lastType;
        };

        [[nodiscard]] int getPointerDepth() const {
            int depth =0 ;
            Type* lastType = pointerElementType;
            while(lastType->isPointerTy()){
                depth++;
                lastType = lastType->getPointerElementType();
            }
            return depth;
        };


        // struct
        [[nodiscard]] bool isStructTy() const { return type == TYPE_NAMES::STRUCT;};
        [[nodiscard]] bool isUnion() const { return structIsUnion;};
        void setMembers(std::vector<NamedVariable> members) {
            structMembers = std::move(members);
        }

        [[nodiscard]] std::vector<NamedVariable> getNamedMembers() const { return structMembers;};
        /*
         * Denotes whether if following all members of the structure would eventually lead down to the same type
         */
        bool isCyclicWithItself = false;


    private:
        TYPE_NAMES type;
        unsigned int integerSize = 0;
        Type* pointerElementType = nullptr;
        Type* arrayElementType = nullptr;;
        uint64_t arraySize = 0;
        std::string structName;
        std::vector<NamedVariable> structMembers;
        bool structIsUnion = false;
    };


    struct NamedVariable{
        NamedVariable(std::string name, Type* type): name(std::move(name)), type(type){};
        Type* type;
        std::string name;
        [[nodiscard]] Type* getType() const { return this->type;};
        [[nodiscard]] std::string getName() const { return this->name;};
    };




    struct GlobalVariable : NamedVariable {
        GlobalVariable(std::string name, Type* type) : NamedVariable(std::move(name), type){};
    };

    struct Argument : NamedVariable {
    public:
        Argument(std::string name, Type* type, bool isPassByValue, bool isSRet) : NamedVariable(std::move(name), type), isPasByValue(isPassByValue), isSRet(isSRet){};
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
        Function(std::string name, Type *retType, std::vector<Argument> arguments, Purity purity);

        std::string name;
        Type *retType;
        std::vector<Argument> arguments; // implicitly ordered
        Purity purity;

        [[nodiscard]] std::string getFunctionSignature() const;
        [[nodiscard]] std::string getPurityName() const;

    private:
        [[nodiscard]] std::string getTypedArgumentNames() const;
    };
}
