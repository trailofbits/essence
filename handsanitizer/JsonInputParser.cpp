#include <iostream>
#include "JsonInputParser.h"

namespace handsanitizer {
    std::string JsonInputParser::getParserRetrievalTextForArguments(std::string jsonInputVariableName, std::vector<Argument> args) {
        std::vector<NamedVariable> a(args.begin(), args.end());
        jsonInputVariableName = jsonInputVariableName + "[\"arguments\"]";
        return getParserRetrievalText(jsonInputVariableName, a, false);
    }

    std::string JsonInputParser::getParserRetrievalTextForGlobals(std::string jsonInputVariableName) {
        std::vector<NamedVariable> globals(this->declarationManager->globals.begin(), this->declarationManager->globals.end());
        jsonInputVariableName = jsonInputVariableName + "[\"globals\"]";
        return getParserRetrievalText(jsonInputVariableName, globals, true);
    }

    std::string JsonInputParser::getParserRetrievalText(const std::string& jsonInputVariableName, const std::vector<NamedVariable>& args,
                                                        bool isForGlobals) {
        std::stringstream s;

        for (auto &a : args) {
            JsonParsingCandidate candidate{
                    .lvalueName = a.getName(),
                    .jsonRoot = jsonInputVariableName + "[\"" + a.getName() + "\"]",
                    .type = a.getType(),
                    .isForGlobalVariable = isForGlobals
            };
            s << getParserRetrievalForNamedType(candidate);
        }
        return s.str();
    }

    std::string JsonInputParser::getParserRetrievalForNamedType(const JsonParsingCandidate &candidate) {
        // arrays only exists in types unless they are global
        if (candidate.isForGlobalVariable && candidate.type->isArrayTy()) {
            return getParserRetrievalForGlobalArrays(candidate);
        }
            // Pointers recurse until they point to a non pointer
            // for a non-pointer it declares the base type and sets it as an array/scalar
        else if (candidate.type->isPointerTy()) {
            return getParserRetrievalForPointers(candidate);
        }

            // if type is scalar => output directly
        else if (candidate.type->isIntegerTy(1) || candidate.type->isIntegerTy(8) || candidate.type->isIntegerTy(16) || candidate.type->isIntegerTy(32) ||
                 candidate.type->isIntegerTy(64) || candidate.type->isFloatTy() || candidate.type->isDoubleTy()) {
            return getParserRetrievalForScalars(candidate);
        }
            // if type is struct, recurse with member names added as prefix
        else if (candidate.type->isStructTy()) {
            return getParserRetrievalForStructs(candidate, false);
        }
        throw std::invalid_argument("could not generate parser for type");
    }

    std::string JsonInputParser::getParserRetrievalForGlobalArrays(const JsonParsingCandidate &candidate) {
        std::stringstream output;
        int arrSize = candidate.type->getArrayNumElements();
        std::string iterator_name = this->declarationManager->getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
        output << "for(int " << iterator_name << " = 0; " << iterator_name << " < " << arrSize << ";" << iterator_name
               << "++) {" << std::endl;

        auto parsedArrayElTypeLvalue = declarationManager->getUniqueTmpCPPVariableNameFor();
        JsonParsingCandidate arrayElTypeCandidate = {.lvalueName = parsedArrayElTypeLvalue,
                .jsonRoot = candidate.jsonRoot + "[" + iterator_name + "]",
                .type = candidate.type->getArrayElementType(),
                .isForGlobalVariable = candidate.isForGlobalVariable,
        };
        output << getParserRetrievalForNamedType(arrayElTypeCandidate); // we declare a new local variable so don't pass in the global flag
        output << candidate.lvalueName << "[" << iterator_name << "] = " << parsedArrayElTypeLvalue << ";" << std::endl;
        output << "}" << std::endl;
        return output.str();
    }


    std::string JsonInputParser::getParserRetrievalForScalars(const JsonParsingCandidate &candidate) {
        std::stringstream output;
        if (!candidate.isForGlobalVariable) // only declare types if its not global, otherwise we introduce them as local variable
            output << candidate.type->getCTypeName() << " ";

        output << candidate.lvalueName;
        output << " = " << candidate.jsonRoot << ".get<" << candidate.type->getCTypeName() << ">();";
        output << std::endl;
        return output.str();
    }

    std::string JsonInputParser::getParserRetrievalForStructs(const JsonParsingCandidate &candidate, bool isMalloced) {
        std::stringstream output;
        //TODO FIX structs
        // first do non array members since these might need to be recursively build up first
        // declare and assign values to struct
        // then fill in struct arrays as these are only available first during struct initialization.
        std::string accessDelimiter = ".";
        if(isMalloced)
            accessDelimiter = "->";

        std::vector<JsonParsingCandidate> parsingCandidatesForMembers;
        for (auto& member : candidate.type->getNamedMembers()) {
            if (!member.getType()->isArrayTy()) {
                JsonParsingCandidate memberCandidate{
                        .lvalueName = declarationManager->getUniqueTmpCPPVariableNameFor(member.getName()),
                        .jsonRoot = candidate.jsonRoot + "[\"" + member.getName() + "\"]",
                        .type = member.getType(),
                        .isForGlobalVariable = false
                };
                output << getParserRetrievalForNamedType(memberCandidate);
                parsingCandidatesForMembers.push_back(memberCandidate);
            }
        }
        // initialize
        if (!candidate.isForGlobalVariable && !isMalloced)
            output << candidate.type->getCTypeName() << " " << candidate.lvalueName << ";" << std::endl;
        int candidateCounter = 0;
        for (int i = 0; i < candidate.type->getNamedMembers().size(); i++) {
            if (!candidate.type->getNamedMembers()[i].getType()->isArrayTy()) {
                output << candidate.lvalueName << accessDelimiter << candidate.type->getNamedMembers()[i].getName() << " = " << parsingCandidatesForMembers[candidateCounter].lvalueName << ";"
                       << std::endl;
                candidateCounter++;
            }
        }


        for (auto& member : candidate.type->getNamedMembers()) {
            // arrays can be also of custom types with arbitrary but finite amounts of nesting, so we need to recurse
            if (member.getType()->isArrayTy()) {
                int arrSize = member.getType()->getArrayNumElements();
                Type *arrType = member.getType()->getArrayElementType();
                std::string iterator_name = this->declarationManager->getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
                output << "for(int " << iterator_name << " = 0; " << iterator_name << " < " << arrSize << ";"
                       << iterator_name << "++) {" << std::endl;

                JsonParsingCandidate arrayMemberCandidate{
                        .lvalueName = declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName + member.getName()),
                        .jsonRoot = candidate.jsonRoot + "[\"" + member.getName() + "\"]" + "[" + iterator_name + "]",
                        .type = arrType,
                        .isForGlobalVariable = false
                };
                output << getParserRetrievalForNamedType(arrayMemberCandidate);
                output << candidate.lvalueName << accessDelimiter << member.getName() << "[" << iterator_name << "] = " << arrayMemberCandidate.lvalueName << ";"
                       << std::endl;
                output << "}" << std::endl;
            }
        }
        return output.str();
    }

    std::string JsonInputParser::getParserRetrievalForPointers(const JsonParsingCandidate &candidate) {
        if (candidate.type->getPointerElementType()->isPointerTy())
            return getParserRetrievalForPointersToPointers(candidate);
        else
            return getParserRetrievalForPointersToNonPointers(candidate);
    }

    //recurse and take stack pointer
    std::string JsonInputParser::getParserRetrievalForPointersToPointers(const JsonParsingCandidate &candidate) {
        std::stringstream output;

        auto lvaluePointee = declarationManager->getUniqueTmpCPPVariableNameFor();
        JsonParsingCandidate pointeeCandidate = JsonParsingCandidate{
                .lvalueName = lvaluePointee,
                .jsonRoot = candidate.jsonRoot,
                .type = candidate.type->getPointerElementType(),
                .isForGlobalVariable = false // recursed members can't be globals
        };

        output << getParserRetrievalForNamedType(candidate);
        if (!candidate.isForGlobalVariable)
            output << candidate.type->getCTypeName() << " ";
        output << candidate.lvalueName << " = &" << lvaluePointee << ";" << std::endl;

        return output.str();
    }


    std::string JsonInputParser::getParserRetrievalForPointersToNonPointers(const JsonParsingCandidate &candidate) {
        if (candidate.type->getPointerElementType()->isIntegerTy(8)) { // char behavior
            return getParserRetrievalForPointerToCharType(candidate);
        } else {
            return getParserRetrievalForPointerToNonCharType(candidate);
        }
    }

    /*
    * we handle chars in 3 different ways, l and rvalues here are in json
    * "char_value" : "string" => null terminated
    * "char_value" : 70 => ascii without null
    * "char_value" : ["f", "o", NULL , "o"] => in memory this should reflect f o NULL o, WITHOUT EXPLICIT NULL TERMINATION!
    * "char_value" : ["fooo", 45, NULL, "long string"], same rules apply here as above, this should not have a null all the way at the end
     *
     * TODO: Probably refactor this
    */
    std::string JsonInputParser::getParserRetrievalForPointerToCharType(const JsonParsingCandidate &candidate) {
        std::stringstream output;
        handsanitizer::Type *pointeeType = candidate.type->getPointerElementType();
        std::string elTypeName = pointeeType->getCTypeName();
        std::string jsonValue = candidate.jsonRoot;

        if (!candidate.isForGlobalVariable)
            output << elTypeName << "* " << candidate.lvalueName << ";" << std::endl;


        output << "if(" << jsonValue << ".is_string()) {" << std::endl;

        std::string candidateLvalueAsString = candidate.lvalueName;

        //we can't use c_str here because if the string itself contains null, then it it is allowed to misbehave
        output << getStringFromJson(candidateLvalueAsString, jsonValue, this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName),
                                    this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName));

        output << "}" << std::endl; // end is_string


        output << "if(" << jsonValue << ".is_number()) {" << std::endl;
        auto tmpLvalueForParsedChar = declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName);
        output << "char " << tmpLvalueForParsedChar << " = " << jsonValue << ".get<char>();" << std::endl;
        output << candidate.lvalueName << " = &" << tmpLvalueForParsedChar << ";" << std::endl;
        output << "}" << std::endl; //end is_number


        output << "if(" << jsonValue << ".is_array()) {" << std::endl;
        auto nestedIterator = this->declarationManager->getUniqueLoopIteratorName();
        auto nestedIteratorIndex = "[" + nestedIterator + "]";
        auto indexedJsonValue = jsonValue + nestedIteratorIndex;
        auto malloc_size_counter = declarationManager->getUniqueTmpCPPVariableNameFor("malloc_size_counter");
        output << "int " << malloc_size_counter << " = 0;" << std::endl;
        output << "for(int " << nestedIterator << " = 0; " << nestedIterator << " < " << jsonValue << ".size(); " << nestedIterator << "++){ " << std::endl;
        output << "if(" << indexedJsonValue << ".is_number() || " << jsonValue << nestedIteratorIndex << ".is_null())" << std::endl;
        output << malloc_size_counter << "++;"; // assume its a char and assume chars take 1 byte
        output << "if(" << indexedJsonValue << ".is_string()){";
        auto str_len_counter = this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName);
        output << getStringLengthFromJson(str_len_counter, indexedJsonValue, // don't trust .size for nulled strings
                                          this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName),
                                          this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName), false);
        output << malloc_size_counter << " =  " << malloc_size_counter << " + " << str_len_counter << ";" << std::endl;
        output << "}" << std::endl; // end is_string


        output << "}" << std::endl; // end for
        // first loop to get the total size
        // then copy

        auto output_buffer_name = this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName);
        auto writeHeadName = this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName); // writes to this offset in the buffer
        auto indexed_output_buffer_name = output_buffer_name + "[" + writeHeadName + "]";
        output << "char* " << output_buffer_name << " = (char*)malloc(sizeof(char) * " << malloc_size_counter << ");"
               << std::endl;
        output << this->declarationManager->registerVariableToBeFreed(output_buffer_name);
        output << "int " << writeHeadName << " = 0;" << std::endl;
        //here copy, loop again over all and write

        output << "for(int " << nestedIterator << " = 0; " << nestedIterator << " < " << jsonValue << ".size(); "
               << nestedIterator << "++){ " << std::endl;
        output << "if(" << jsonValue << nestedIteratorIndex << ".is_string()){ ";
        auto str_val = this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName);
        output << "std::string " << str_val << " = " << indexedJsonValue << ".get<std::string>();";
        // copy the string into the correct position
        // .copy guarantees to not add an extra null char at the end which is what we desire
        auto charsWritten = this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName);
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

        output << candidate.lvalueName << " = " << output_buffer_name << ";"
               << std::endl;
        output << "}" << std::endl; // end is_array

        return output.str();
    }

    std::string JsonInputParser::getParserRetrievalForPointerToNonCharType(const JsonParsingCandidate &candidate) {
        std::stringstream output;

        handsanitizer::Type *pointeeType = candidate.type->getPointerElementType();
        std::string elTypeName = pointeeType->getCTypeName();
        std::string jsonValue = candidate.jsonRoot;

        std::string argument_name = candidate.lvalueName;
        if (!candidate.isForGlobalVariable)
            output << elTypeName << "* " << argument_name << ";" << std::endl;

        if (pointeeType->isStructTy()) {
            auto cmp = [&candidate](const std::pair<Type*, std::string>& pair){ return candidate.type->getPointerElementType() == pair.first;};
            auto helperFunc = std::find_if(structParsingHelperFunctions.begin(), structParsingHelperFunctions.end(), cmp);
            if(helperFunc == structParsingHelperFunctions.end())
                throw std::invalid_argument("Trying to parse a structure for which no helper function exists");


            auto funcName = helperFunc->second;
            output << candidate.lvalueName << " = " << funcName << "(" << candidate.jsonRoot << ");" << std::endl;
        } else {
            // if it's an array
            output << "if(" << jsonValue << ".is_array()) { " << std::endl;
            std::string malloced_value = declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName + "_malloced");
            std::string arrSize = candidate.jsonRoot + ".size()";

            output << elTypeName << "* " << malloced_value << " = (" << elTypeName << "*) malloc(sizeof(" << elTypeName << ") * " << arrSize << ");"
                   << std::endl;
            output << declarationManager->registerVariableToBeFreed(malloced_value);

            std::string iterator_name = declarationManager->getUniqueLoopIteratorName();
            output << "for(int " << iterator_name << " = 0; " << iterator_name << " < " << arrSize << ";" << iterator_name << "++) {" << std::endl;
            output << malloced_value << "[" << iterator_name << "] = " << jsonValue << "[" << iterator_name << "].get<" << elTypeName << ">();" << std::endl;
            output << "}" << std::endl; //endfor

            output << argument_name << " = " << malloced_value << ";" << std::endl;
            output << "}" << std::endl; //end if_array

            // if its a number
            output << "if(" << jsonValue << ".is_number()){" << std::endl;
            //cast rvalue in case its a char
            auto malloc_name = this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalueName);
            output << elTypeName << "* " << malloc_name << " = (" << elTypeName << "*) malloc(sizeof(" << elTypeName
                   << "));" << std::endl;
            output << this->declarationManager->registerVariableToBeFreed(malloc_name);
            output << malloc_name << "[0] = (" << elTypeName << ")" << jsonValue << ".get<" << elTypeName << ">();"
                   << std::endl;
            output << candidate.lvalueName << " = " << malloc_name << ";" << std::endl;
            output << "}" << std::endl; //end if number
        }
        return output.str();
    }


    std::string JsonInputParser::getStringFromJson(const std::string& output_var, const std::string& json_name, const std::string& tmp_name_1,
                                                   const std::string& tmp_name_2, bool addNullTermination) {
        std::stringstream s;
        //TODO Reimplement null termination
        s << "std::string " << tmp_name_1 << " = " << json_name << ".get<std::string>();" << std::endl;
        s << output_var << " = " << tmp_name_1 << ".data();";
        return s.str();
    }

    std::string JsonInputParser::getStringLengthFromJson(const std::string& output_var, const std::string& json_name, const std::string& tmp_name_1,
                                                         const std::string& tmp_name_2, bool addNullTermination) {
        std::stringstream s;
        s << "std::string " << tmp_name_1 << " = " << json_name << ".get<std::string>();" << std::endl;
        s << "std::vector<char> " << tmp_name_2 << "(" << tmp_name_1 << ".begin(), " << tmp_name_1 << ".end());";
        if (addNullTermination)
            s << tmp_name_2 << ".push_back('\x00');" << std::endl;
        s << "int " << output_var << " = " << tmp_name_2 << ".size();" << std::endl;
        return s.str();
    }


    /*
     * So this function works a little bit differently
     * instead of having a prefixes which denotes the entire path we have a json variable which handles the recursion + tail
     *
     * Everything is just a little too different to reuse the other struct parsing function :(
     *
     * maybe add in the interface that this only works for pointer to structs?
     */
    std::string JsonInputParser::getPointerToStructParserFunctionDefinitions(Type *type) {
        //TODO fix this
        std::stringstream output;
        std::string jsonRoot = declarationManager->getUniqueTmpCPPVariableNameFor("json");
        auto cmp = [&type](const std::pair<Type*, std::string>& pair){ return type == pair.first;};
        auto helperFunc = std::find_if(structParsingHelperFunctions.begin(), structParsingHelperFunctions.end(), cmp);


        output << type->getCTypeName() << "* " << helperFunc->second << "(const nlohmann::json& " << jsonRoot << ") {" << std::endl;



        output << "if(" << jsonRoot << ".is_null()) { return nullptr; } else {" << std::endl;

        auto localLvalue = declarationManager->getUniqueTmpCPPVariableNameFor();

        output << type->getCTypeName() << "* " << localLvalue << " = (" << type->getCTypeName() << "*) malloc(sizeof(" << type->getCTypeName() << "));" << std::endl;
        auto pointeeCandidate = JsonParsingCandidate{
            .lvalueName = localLvalue,
            .jsonRoot = jsonRoot,
            .type = type,
            .isForGlobalVariable = false
        };
        if(!pointeeCandidate.type->isStructTy())
            throw std::invalid_argument("ugh");
        output << getParserRetrievalForStructs(pointeeCandidate, true) << std::endl;
        output << "return " << localLvalue << ";" << std::endl;
        output << "}" << std::endl; // end else for non-null struct

        output << "}" << std::endl; // end function

        //TODO make sure that this can't be called at wrong time

        return output.str();
    }

    std::string JsonInputParser::getStructParsingHelpers() {
        std::stringstream output;
        //first declare then define

        for(auto& udt : declarationManager->userDefinedTypes){
            auto funcName = declarationManager->getUniqueTmpCPPVariableNameFor("parse" + udt->getCTypeName() + "Struct");
            structParsingHelperFunctions.emplace_back(udt, funcName);
            output << udt->getCTypeName() << "* " << funcName << "(const nlohmann::json& " << declarationManager->getUniqueTmpCPPVariableNameFor() << ");" << std::endl;
        }

        for(auto& udt : declarationManager->userDefinedTypes){
            output << getPointerToStructParserFunctionDefinitions(udt);
        }
        return output.str();
    }
}

