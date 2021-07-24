//
// Created by sabastiaan on 19-07-21.
//

#include "include/name_generation.hpp"




std::string CPP_ADDRESSING_DELIMITER = ".";
std::string LVALUE_DELIMITER = "_";
std::string POINTER_DENOTATION = "__p";


int iterator_names_used = 0;
std::vector<std::string> iterator_names;
std::string getUniqueLoopIteratorName(){
    std::string name = "i" + std::to_string(iterator_names_used);
    iterator_names.push_back(name);
    iterator_names_used++;
    return name;
}



std::string joinStrings(std::vector<std::string> strings, StringJoiningFormat format){
    std::stringstream output;
    std::string delimiter = "";
    if(format == GENERATE_FORMAT_CPP_ADDRESSING)
        delimiter = CPP_ADDRESSING_DELIMITER;
    if(format == GENERATE_FORMAT_CPP_VARIABLE)
        delimiter = LVALUE_DELIMITER;


    for(std::string s : strings){
        if(format != GENERATE_FORMAT_CPP_VARIABLE && s == POINTER_DENOTATION)
            continue; // don't print the pointer markings in lvalue as these are abstracted away
        if(format == GENERATE_FORMAT_JSON_ARRAY_ADDRESSING){
            if(std::find(iterator_names.begin(), iterator_names.end(), s) != iterator_names.end())
                output << "[" << s << "]";
            else
                output << "[\"" << s << "\"]";
        }
        else{
            output << s << delimiter;
        }
    }

    auto retstring = output.str();
    if(retstring.length() > 0 && format != GENERATE_FORMAT_JSON_ARRAY_ADDRESSING)
        retstring.pop_back(); //remove trailing delimiter
    return retstring;
}



int getNum(){ return 1;}
// Adds itself in the correct order to definitionStream
//void defineIfNeeded(llvm::Type* arg, bool isRetType){
//    // if we have a T**** we might still need to define T, so recurse instead of check
//    std::stringstream definitionStrings;
//    if(arg->isPointerTy())
//        defineIfNeeded(arg->getPointerElementType(), isRetType);
//
////    std::cout << "called for: " << getCTypeNameForLLVMType(arg);
//    if(arg->isStructTy()){ // we don't care about arrays thus no (is aggregrate type)
//        std::cout << " Found a struct type";
//
//        if(isStructAlreadyDefined((llvm::StructType*) arg))
//            return;
//        std::cout << "Not defined";
//        if(arg->getStructNumElements() > 0){
//            std::cout << "Has members";
//            std::stringstream  elementsString;
//
//            if(arg->getStructName().find("union.") != std::string::npos)
//                newDefinedStruct.isUnion = true;
//
//            // all members need to be added, if any struct is referenced we need tto define it again
//            for(int i =0 ; i < arg->getStructNumElements(); i++){
//                auto child = arg->getStructElementType(i);
//
//                /*
//                 * A struct can contain 3 constructs which we might need to define
//                 * 1. pointer should always be tried to be defined
//                 * 2. a struct obviously
//                 * 3. array types
//                 */
//                if (child->isStructTy() || child->isPointerTy())
//                    defineIfNeeded(child);
//                if (child->isArrayTy()) {
//                    auto arrType = child->getArrayElementType();
//                    if (arrType->isStructTy() || arrType->isPointerTy())
//                        defineIfNeeded(arrType);
//                }
//
//                std::string childname = "e_" + std::to_string(i);
//                if(child->isArrayTy()){ // of course we can't have nice things in this world :(
//                    auto arrayType = child->getArrayElementType();
//                    auto arraySize = child->getArrayNumElements();
//                    elementsString << "\t" << getCTypeNameForLLVMType(arrayType) << " " << childname  << "[" << arraySize << "];" << std::endl;
//                }
//                else{
//                    elementsString << "\t" << getCTypeNameForLLVMType(child) << " " << childname  << ";" << std::endl;
//                }
//
//                newDefinedStruct.member_names.push_back(childname);
//            }
//
//            // TODO: use getCPPName instead of getStructName?
//            auto structName = getCTypeNameForLLVMType(arg);
//            if(isRetType)
//                structName = "returnType";
//            newDefinedStruct.structType = (llvm::StructType*) arg;
//            if(newDefinedStruct.isUnion)
//                definitionStrings << "typedef union ";
//            else
//                definitionStrings << "typedef struct ";
//            definitionStrings << structName << " { " << std::endl << elementsString.str() << "} " << structName << ";" <<  std::endl <<  std::endl;
//            registerStruct(definitionStrings.str());
//            newDefinedStruct.definedCStructName = structName;
//            definedStructs.push_back(newDefinedStruct);
//        }
//    }
//}

//
//std::string getCTypeNameForLLVMType(handsanitizer::Type* type){
//    if(type->isPointerTy())
//        return getCTypeNameForLLVMType(type->getPointerElementType()) + "*";
//
////    bools get converted to i8 in llvm
////    if(type->isIntegerTy(1))
////        return "bool";
//
//    if(type->isIntegerTy(8))
//        return "char";
//
//    if(type->isIntegerTy(16))
//        return "int16_t";
//
//    if(type->isIntegerTy(32))
//        return "int32_t";
//
//    if(type->isIntegerTy(64))
//        return "int64_t";
//
////    if(type->isIntegerTy(64))
////        return "long long"; // this makes very strong assumptions on underling structures, should fix this
//
//    if(type->isVoidTy())
//        return "void";
//
//    if(type->isFloatTy())
//        return "float";
//
//    if(type->isFloatTy())
//        return "double";
//
//    if(type->isStructTy()){
//        std::string s = type->getStructName();
//        if(s.find("struct.") != std::string::npos)
//            s = s.replace(0, 7, ""); //removes struct. prefix
//
//        else if (s.find("union.") != std::string::npos)
//            s = s.replace(0, 6, ""); //removes union. prefix
//        return s;
//    }
//
//    if(type->isArrayTy())
//        throw std::invalid_argument("Arrays can't have their names expressed like this");
//
//    return "Not supported";
//}

std::string getRegisteredStructs(){
    return "";
}

void registerStruct(std::string s){

}
