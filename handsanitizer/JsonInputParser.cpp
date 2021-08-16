//
// Created by sabastiaan on 11-08-21.
//

#include "JsonInputParser.h"
#include <sstream>
namespace handsanitizer{

std::string
JsonInputParser::getParserRetrievalTextForArguments(std::string jsonInputVariableName, std::vector<Argument> args) {
    std::vector<NamedVariable> a(args.begin(), args.end());
    jsonInputVariableName = jsonInputVariableName + "[\"arguments\"]";
    return getParserRetrievalText(jsonInputVariableName, a, false);
}

std::string JsonInputParser::getParserRetrievalTextForGlobals(std::string jsonInputVariableName) {
    std::vector<NamedVariable> globals(this->declarationManager->globals.begin(), this->declarationManager->globals.end());
    jsonInputVariableName = jsonInputVariableName + "[\"globals\"]";
    return getParserRetrievalText(jsonInputVariableName, globals, true);
}


std::string JsonInputParser::getParserRetrievalText(std::string jsonInputVariableName, std::vector<NamedVariable> args,
                                           bool isForGlobals) {
    std::stringstream s;

    for (auto &a : args) {
        std::vector<std::string> dummy;
        dummy.push_back(a.getName());
        s << getParserRetrievalForNamedType(jsonInputVariableName, dummy, a.type, isForGlobals);
    }
    return s.str();
}



std::string
JsonInputParser::getParserRetrievalForNamedType(std::string jsonInputVariableName, std::vector<std::string> prefixes,
                                       handsanitizer::Type *type,
                                       bool isForGlobals) {
    std::stringstream output;

    // arrays only exists in types unless they are global
    if (isForGlobals && type->isArrayTy()) {
        return getParserRetrievalForGlobalArrays(jsonInputVariableName, prefixes, type);
    }
        // Pointers recurse until they point to a non pointer
        // for a non-pointer it declares the base type and sets it as an array/scalar
    else if (type->isPointerTy()) {
        return getParserRetrievalForPointers(jsonInputVariableName, prefixes, type, isForGlobals);
    }

        // if type is scalar => output directly
    else if (type->isIntegerTy(1) || type->isIntegerTy(8) || type->isIntegerTy(16) || type->isIntegerTy(32) ||
             type->isIntegerTy(64) || type->isFloatTy() || type->isDoubleTy()) {
        return getParserRetrievalForScalars(jsonInputVariableName, prefixes, type, isForGlobals);
    }

        // if type is struct, recurse with member names added as prefix
    else if (type->isStructTy()) {
        return getParserRetrievalForStructs(jsonInputVariableName, prefixes, type, isForGlobals);
    }
    return output.str();
}

std::string JsonInputParser::getParserRetrievalForGlobalArrays(std::string jsonInputVariableName,
                                                      std::vector<std::string> prefixes,
                                                      handsanitizer::Type *type) {
    std::stringstream output;
    int arrSize = type->getArrayNumElements();
    handsanitizer::Type *arrType = type->getArrayElementType();
    std::string iterator_name = this->declarationManager->getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
    output << "for(int " << iterator_name << " = 0; " << iterator_name << " < " << arrSize << ";" << iterator_name
           << "++) {" << std::endl;
    std::vector<std::string> fullname(prefixes);
    fullname.push_back(iterator_name);
    output << getParserRetrievalForNamedType(jsonInputVariableName, fullname, arrType,
                                             false); // we declare a new local variable so don't pass in the global flag
    output << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << "[" << iterator_name << "] = "
           << this->declarationManager->joinStrings(fullname, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;
    output << "}" << std::endl;
    return output.str();
}




std::string
JsonInputParser::getParserRetrievalForScalars(std::string jsonInputVariableName, std::vector<std::string> prefixes,
                                     handsanitizer::Type *type, bool isForGlobals) {
    std::stringstream output;
    if (!isForGlobals) // only declare types if its not global, otherwise we introduce them as local variable
        output << type->getCTypeName() << " ";

    output << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE);
    output << " = " << jsonInputVariableName << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING)
           << ".get<" << type->getCTypeName() << ">();";
    output << std::endl;
    return output.str();
}

std::string
JsonInputParser::getParserRetrievalForStructs(std::string jsonInputVariableName, std::vector<std::string> prefixes,
                                     handsanitizer::Type *type, bool isForGlobals) {
    std::stringstream output;
    // first do non array members since these might need to be recursively build up first
    // declare and assign values to struct
    // then fill in struct arrays as these are only available first during struct initialization.

    for (auto member : type->getNamedMembers()) {
        if (member.getType()->isArrayTy() == false) {
            std::vector<std::string> fullMemberName(prefixes);
            fullMemberName.push_back(member.getName());
            output << getParserRetrievalForNamedType(jsonInputVariableName, fullMemberName, member.getType(),
                                                     false);
        }
    }
    output << "\t";
    if (!isForGlobals)
        output << type->getCTypeName() << " ";
    output << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE);
    if (isForGlobals)
        output << " = { " << std::endl; // with globals we have different syntax :?
    else
        output << "{ " << std::endl;
    for (auto member : type->getNamedMembers()) {
        if (member.getType()->isArrayTy() == false) {
            std::vector<std::string> fullMemberName(prefixes);
            fullMemberName.push_back(member.getName());
            output << "\t\t" << "." << member.getName() << " = "
                   << this->declarationManager->joinStrings(fullMemberName, GENERATE_FORMAT_CPP_VARIABLE) << "," << std::endl;
        }
    }
    output << "\t};" << std::endl;

    for (auto member : type->getNamedMembers()) {
        // arrays can be also of custom types with arbitrary but finite amounts of nesting, so we need to recurse
        if (member.getType()->isArrayTy()) {
            int arrSize = member.getType()->getArrayNumElements();
            Type *arrType = member.getType()->getArrayElementType();
            std::string iterator_name = this->declarationManager->getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
            output << "for(int " << iterator_name << " = 0; " << iterator_name << " < " << arrSize << ";"
                   << iterator_name << "++) {" << std::endl;
            std::vector<std::string> fullMemberName(prefixes);
            fullMemberName.push_back(member.getName());
            fullMemberName.push_back(iterator_name);
            output << getParserRetrievalForNamedType(jsonInputVariableName, fullMemberName, arrType, false);
            output << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << "." << member.getName() << "["
                   << iterator_name << "] = " << this->declarationManager->joinStrings(fullMemberName, GENERATE_FORMAT_CPP_VARIABLE) << ";"
                   << std::endl;
            output << "}" << std::endl;
        }
    }
    return output.str();
}

std::string
JsonInputParser::getParserRetrievalForPointers(std::string jsonInputVariableName, std::vector<std::string> prefixes,
                                      handsanitizer::Type *type, bool isForGlobals) {
    if (type->getPointerElementType()->isPointerTy())
        return getParserRetrievalForPointersToPointers(jsonInputVariableName, prefixes, type, isForGlobals);
    else
        return getParserRetrievalForPointersToNonPointers(jsonInputVariableName, prefixes, type, isForGlobals);
}

//recurse and take stack pointer
std::string JsonInputParser::getParserRetrievalForPointersToPointers(std::string jsonInputVariableName,
                                                            std::vector<std::string> prefixes,
                                                            handsanitizer::Type *type, bool isForGlobals) {
    std::stringstream output;
    std::vector<std::string> referenced_name(prefixes);
    referenced_name.push_back(POINTER_DENOTATION);
    output << getParserRetrievalForNamedType(jsonInputVariableName, referenced_name, type->getPointerElementType(),
                                             isForGlobals);
    if (!isForGlobals)
        output << type->getCTypeName() << " ";
    output << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = &"
           << this->declarationManager->joinStrings(referenced_name, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;
    return output.str();
}


std::string JsonInputParser::getParserRetrievalForPointersToNonPointers(std::string jsonInputVariableName,
                                                               std::vector<std::string> prefixes,
                                                               handsanitizer::Type *type, bool isForGlobals) {
    if (type->getPointerElementType()->isIntegerTy(8)) { // char behavior
        return getParserRetrievalForPointerToCharType(jsonInputVariableName, prefixes, type, isForGlobals);
    } else {
        return getParserRetrievalForPointerToNonCharType(jsonInputVariableName, prefixes, type, isForGlobals);
    }
}

/*
* we handle chars in 3 different ways, l and rvalues here are in json
* "char_value" : "string" => null terminated
* "char_value" : 70 => ascii without null
* "char_value" : ["f", "o", NULL , "o"] => in memory this should reflect f o NULL o, WITHOUT EXPLICIT NULL TERMINATION!
* "char_value" : ["fooo", 45, NULL, "long string"], same rules apply here as above, this should not have a null all the way at the end
*/
std::string
JsonInputParser::getParserRetrievalForPointerToCharType(std::string jsonInputVariableName, std::vector<std::string> prefixes,
                                               handsanitizer::Type *type, bool isForGlobals) {
    std::stringstream output;
    handsanitizer::Type *pointeeType = type->getPointerElementType();
    std::string elTypeName = pointeeType->getCTypeName();
    std::string jsonValue = jsonInputVariableName + this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING);

    if (!isForGlobals)
        output << elTypeName << "* " << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;



    output << "if(" << jsonValue << ".is_string()) {" << std::endl;

    std::string string_tmp_val = this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE);

    //we can't use c_str here because if the string itself contains null, then it it is allowed to misbehave
    output << getStringFromJson(string_tmp_val, jsonValue, this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes),
                                this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes));

    output << "}" << std::endl; // end is_string


    output << "if(" << jsonValue << ".is_number()) {" << std::endl;
    auto tmp_char_val = this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes);
    output << "char " << tmp_char_val << " = " << jsonValue << ".get<char>();" << std::endl;
    output << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = &" << tmp_char_val << ";" << std::endl;
    output << "}" << std::endl; //end is_number


    output << "if(" << jsonValue << ".is_array()) {" << std::endl;
    auto nestedIterator = this->declarationManager->getUniqueLoopIteratorName();
    auto nestedIteratorIndex = "[" + nestedIterator + "]";
    auto indexedJsonValue = jsonValue + nestedIteratorIndex;
    output << "int malloc_size_counter = 0;" << std::endl;
    output << "for(int " << nestedIterator << " = 0; " << nestedIterator << " < " << jsonValue << ".size(); "
           << nestedIterator << "++){ " << std::endl;
    output << "if(" << indexedJsonValue << ".is_number() || " << jsonValue << nestedIteratorIndex
           << ".is_null()) malloc_size_counter++;"; // assume its a char
    output << "if(" << indexedJsonValue << ".is_string()){"; // assume its a byte
    auto str_len_counter = this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes);
    output << getStringLengthFromJson(str_len_counter, indexedJsonValue, // don't trust .size for nulled strings
                                      this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes),
                                      this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes), false);
    output << "malloc_size_counter = malloc_size_counter + " << str_len_counter << ";" << std::endl;
    output << "}" << std::endl; // end is_string


    output << "}" << std::endl; // end for
    // first loop to get the total size
    // then copy

    auto output_buffer_name = this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes);
    auto writeHeadName = this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes); // writes to this offset in the buffer
    auto indexed_output_buffer_name = output_buffer_name + "[" + writeHeadName + "]";
    output << "char* " << output_buffer_name << " = (char*)malloc(sizeof(char) * malloc_size_counter);"
           << std::endl;
    output << this->declarationManager->registerVariableToBeFreed(output_buffer_name);
    output << "int " << writeHeadName << " = 0;" << std::endl;
    //here copy, loop again over all and write

    output << "for(int " << nestedIterator << " = 0; " << nestedIterator << " < " << jsonValue << ".size(); "
           << nestedIterator << "++){ " << std::endl;
    output << "if(" << jsonValue << nestedIteratorIndex << ".is_string()){ ";
    auto str_val = this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes);
    output << "std::string " << str_val << " = " << indexedJsonValue << ".get<std::string>();";
    // copy the string into the correct position
    // .copy guarantees to not add an extra null char at the end which is what we desire
    auto charsWritten = this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes);
    output << "auto " << charsWritten << " = " << str_val << ".copy(&" << indexed_output_buffer_name << ", "
           << str_val << ".size(), " << "0);" << std::endl;
    output << writeHeadName << " = " << writeHeadName << " + " << charsWritten << ";" << std::endl;
    output << "}" << std::endl; // end is string

    output << "if(" << jsonValue << nestedIteratorIndex << ".is_null()){ ";
    output << indexed_output_buffer_name << " = '\\x00';" << std::endl;
    output << writeHeadName << "++;" << std::endl;
    output << "}" << std::endl; // end is string

    output << "if(" << jsonValue << nestedIteratorIndex << ".is_number()){ ";

    output << indexed_output_buffer_name << " = (char)" << indexedJsonValue << ".get<int>();" << std::endl;
    output << writeHeadName << "++;" << std::endl;
    output << "}" << std::endl; // end is string


    output << "}" << std::endl; // end for

    output << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = " << output_buffer_name << ";"
           << std::endl;
    output << "}" << std::endl; // end is_array

    return output.str();
}

std::string JsonInputParser::getParserRetrievalForPointerToNonCharType(std::string jsonInputVariableName,
                                                              std::vector<std::string> prefixes,
                                                              handsanitizer::Type *type, bool isForGlobals) {
    std::stringstream output;
    handsanitizer::Type *pointeeType = type->getPointerElementType();
    std::string elTypeName = pointeeType->getCTypeName();
    std::string jsonValue = jsonInputVariableName + this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING);

    std::string argument_name = this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE);
    if (!isForGlobals)
        output << elTypeName << "* " << argument_name << ";" << std::endl;

    if(pointeeType->isStructTy()){
        std::vector<std::string> struct_name = std::vector<std::string>(prefixes);
        struct_name.push_back(POINTER_DENOTATION);
        output << getParserRetrievalForStructs(jsonInputVariableName, struct_name, pointeeType, isForGlobals) << std::endl;
        output << argument_name << " = &" << this->declarationManager->joinStrings(struct_name, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;
    }
    else{
        // if its an array
        output << "if(" << jsonValue << ".is_array()) { " << std::endl;
        std::string malloced_value = this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes);
        std::string arrSize = jsonInputVariableName + this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING) + ".size()";

        output << elTypeName << "* " << malloced_value << " = (" << elTypeName << "*) malloc(sizeof(" << elTypeName << ") * " << arrSize << ");" << std::endl;
        output << this->declarationManager->registerVariableToBeFreed(malloced_value);

        std::string iterator_name = this->declarationManager->getUniqueLoopIteratorName();
        output << "for(int " << iterator_name << " = 0; " << iterator_name << " < " << arrSize << ";" << iterator_name << "++) {" << std::endl;
        output << malloced_value << "[" << iterator_name << "] = " << jsonValue << "[" << iterator_name << "].get<" << elTypeName << ">();" << std::endl;
        output << "}" << std::endl; //endfor

        output << argument_name << " = " << malloced_value << ";" << std::endl;
        output << "}" << std::endl; //end if_array

        // if its a number
        output << "if(" << jsonValue << ".is_number()){" << std::endl;
        //cast rvalue in case its a char
        auto malloc_name = this->declarationManager->getUniqueTmpCPPVariableNameFor(prefixes);
        output << elTypeName << "* " << malloc_name << " = (" << elTypeName << "*) malloc(sizeof(" << elTypeName
               << "));" << std::endl;
        output << this->declarationManager->registerVariableToBeFreed(malloc_name);
        output << malloc_name << "[0] = (" << elTypeName << ")" << jsonValue << ".get<" << elTypeName << ">();"
               << std::endl;
        output << this->declarationManager->joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = " << malloc_name << ";" << std::endl;
        output << "}" << std::endl; //end if number
    }
    return output.str();
}




std::string JsonInputParser::getStringFromJson(std::string output_var, std::string json_name, std::string tmp_name_1,
                                      std::string tmp_name_2, bool addNullTermination) {
    std::stringstream s;
    s << "std::string " << tmp_name_1 << " = " << json_name << ".get<std::string>();" << std::endl;
    s << output_var << " = " << tmp_name_1 << ".data();";
    return s.str();
}

std::string JsonInputParser::getStringLengthFromJson(std::string output_var, std::string json_name, std::string tmp_name_1,
                                            std::string tmp_name_2, bool addNullTermination) {
    std::stringstream s;
    s << "std::string " << tmp_name_1 << " = " << json_name << ".get<std::string>();" << std::endl;
    s << "std::vector<char> " << tmp_name_2 << "(" << tmp_name_1 << ".begin(), " << tmp_name_1 << ".end());";
    if(addNullTermination)
        s << tmp_name_2 << ".push_back('\x00');" << std::endl;
    s << "int " << output_var << " = " << tmp_name_2 << ".size();" << std::endl;
    return s.str();
}

}

