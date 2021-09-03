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
                .lvalue = NamedVariable(a.getName(), a.getType()),
                .jsonRoot = jsonInputVariableName + "[\"" + a.getName() + "\"]",
                .isForGlobalVariable = isForGlobals
            };
            s << getParserRetrievalForNamedType(candidate);
        }
        return s.str();
    }

    std::string JsonInputParser::getParserRetrievalForNamedType(const JsonParsingCandidate &candidate) {
        // arrays only exists in types, but members might be constructed through a call to this function
        if (candidate.lvalue.getType()->isArrayTy()) {
            return getParserRetrievalForGlobalArrays(candidate);
        }
            // Pointers recurse until they point to a non pointer
            // for a non-pointer it declares the base type and sets it as an array/scalar
        else if (candidate.lvalue.getType()->isPointerTy()) {
            return getParserRetrievalForPointers(candidate);
        }

            // if type is scalar => output directly
        else if (candidate.lvalue.getType()->isIntegerTy(1) || candidate.lvalue.getType()->isIntegerTy(8) || candidate.lvalue.getType()->isIntegerTy(16) || candidate.lvalue.getType()->isIntegerTy(32) ||
                 candidate.lvalue.getType()->isIntegerTy(64) || candidate.lvalue.getType()->isFloatTy() || candidate.lvalue.getType()->isDoubleTy()) {
            return getParserRetrievalForScalars(candidate);
        }
            // if type is struct, recurse with member names added as prefix
        else if (candidate.lvalue.getType()->isStructTy()) {
            return getParserRetrievalForStructs(candidate, false);
        }
        throw std::invalid_argument("could not generate parser for type " + candidate.lvalue.getNameWithType());
    }

    std::string JsonInputParser::getParserRetrievalForGlobalArrays(const JsonParsingCandidate &candidate) {
        /*
         * if this is an multi dimensional array we need to handle it in one call instead of multiple recursive calls
         * as we actually need to instantiate only one variable for all arrays together
         * this is fine as each array contains the same base type anyway
         */
        std::stringstream output;

        std::vector<std::pair<std::string, int>> iteratorNameAndLength;
        auto lastType = candidate.lvalue.getType();
        while(lastType->isArrayTy()){
            std::string iterator_name = this->declarationManager->getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
            int arrSize = lastType->getArrayNumElements();
            iteratorNameAndLength.emplace_back(iterator_name, arrSize);
            lastType = lastType->getArrayElementType();
        }

        for(auto& itNameLengthPair: iteratorNameAndLength)
            output << "for(int " << itNameLengthPair.first << " = 0; " << itNameLengthPair.first << " < " << itNameLengthPair.second << ";" << itNameLengthPair.first << "++) {" << std::endl;

        auto parsedArrayElTypeLvalue = declarationManager->getUniqueTmpCPPVariableNameFor();
        auto arrElLvalue = NamedVariable(parsedArrayElTypeLvalue, lastType);

        std::stringstream arrayIndexingForJson;
        arrayIndexingForJson << "[\"value\"]";
        for(auto& indexing: iteratorNameAndLength){
            if(indexing != iteratorNameAndLength.back())
                arrayIndexingForJson << "[" + indexing.first + "]" + "[\"value\"]";
            else
                arrayIndexingForJson << "[" + indexing.first + "]";
        }

        std::stringstream arrayIndexingForLvalue;
        for(auto& indexing: iteratorNameAndLength){
            arrayIndexingForLvalue << "[" + indexing.first + "]";
        }


        JsonParsingCandidate arrayElTypeCandidate = {
                .lvalue = arrElLvalue,
                .jsonRoot = candidate.jsonRoot + arrayIndexingForJson.str(),
                .isForGlobalVariable = false,
                };
        output << getParserRetrievalForNamedType(arrayElTypeCandidate); // we declare a new local variable so don't pass in the global flag


        output << candidate.lvalue.getName() << arrayIndexingForLvalue.str() << " = " << parsedArrayElTypeLvalue << ";" << std::endl;


        for(auto& itNameLengthPair: iteratorNameAndLength)
            output << "}" << std::endl;

        return output.str();
    }


    std::string JsonInputParser::getParserRetrievalForScalars(const JsonParsingCandidate &candidate) {
        std::stringstream output;
        if (!candidate.isForGlobalVariable) // only declare types if its not global, otherwise we introduce them as local variable
            output << candidate.lvalue.getType()->getCTypeName() << " ";

        output << candidate.lvalue.getName();
        output << " = " << candidate.jsonRoot << "[\"value\"].get<" << candidate.lvalue.getType()->getCTypeName() << ">();";
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
        for (auto& member : candidate.lvalue.getType()->getNamedMembers()) {
            if (!member.getType()->isArrayTy()) {
                JsonParsingCandidate memberCandidate{
                    .lvalue = NamedVariable(declarationManager->getUniqueTmpCPPVariableNameFor(member.getName()), member.getType()),
                    .jsonRoot = candidate.jsonRoot + "[\"value\"][\"" + member.getName() + "\"]",
                    .isForGlobalVariable = false
                };
                output << getParserRetrievalForNamedType(memberCandidate);
                parsingCandidatesForMembers.push_back(memberCandidate);
            }
        }
        // initialize
        if (!candidate.isForGlobalVariable && !isMalloced)
            output << candidate.lvalue.getType()->getCTypeName() << " " << candidate.lvalue.getName() << ";" << std::endl;
        int candidateCounter = 0;
        for (int i = 0; i < candidate.lvalue.getType()->getNamedMembers().size(); i++) {
            if (!candidate.lvalue.getType()->getNamedMembers()[i].getType()->isArrayTy()) {
                output << candidate.lvalue.getName() << accessDelimiter << candidate.lvalue.getType()->getNamedMembers()[i].getName() << " = " << parsingCandidatesForMembers[candidateCounter].lvalue.getName() << ";"
                       << std::endl;
                candidateCounter++;
            }
        }


        for (auto& member : candidate.lvalue.getType()->getNamedMembers()) {
            // arrays can be also of custom types with arbitrary but finite amounts of nesting, so we need to recurse
            if (member.getType()->isArrayTy()) {
                int arrSize = member.getType()->getArrayNumElements();
                Type *arrType = member.getType()->getArrayElementType();
                std::string iterator_name = this->declarationManager->getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
                output << "for(int " << iterator_name << " = 0; " << iterator_name << " < " << arrSize << ";" << iterator_name << "++) {" << std::endl;

                auto newLvalueName = declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName() + member.getName());
                JsonParsingCandidate arrayMemberCandidate{
                    .lvalue = NamedVariable(newLvalueName, arrType),
                    .jsonRoot = candidate.jsonRoot + "[\"value\"][\"" + member.getName() + "\"]" + "[\"value\"][" + iterator_name + "]",
                    .isForGlobalVariable = false
                };
                output << getParserRetrievalForNamedType(arrayMemberCandidate);
                output << candidate.lvalue.getName() << accessDelimiter << member.getName() << "[" << iterator_name << "] = " << arrayMemberCandidate.lvalue.getName() << ";"
                       << std::endl;
                output << "}" << std::endl;
            }
        }
        return output.str();
    }

    std::string JsonInputParser::getParserRetrievalForPointers(const JsonParsingCandidate &candidate) {
        if (candidate.lvalue.getType()->getPointerElementType()->isPointerTy())
            return getParserRetrievalForPointersToPointers(candidate);
        else
            return getParserRetrievalForPointersToNonPointers(candidate);
    }

    //recurse and take stack pointer
    std::string JsonInputParser::getParserRetrievalForPointersToPointers(const JsonParsingCandidate &candidate) {
        std::stringstream output;

        auto lvaluePointee = declarationManager->getUniqueTmpCPPVariableNameFor();
        JsonParsingCandidate pointeeCandidate = JsonParsingCandidate{
            .lvalue = NamedVariable(lvaluePointee, candidate.lvalue.getType()->getPointerElementType()),
            .jsonRoot = candidate.jsonRoot,
            .isForGlobalVariable = false // recursed members can't be globals
        };

        output << getParserRetrievalForNamedType(pointeeCandidate);
        if (!candidate.isForGlobalVariable)
            output << candidate.lvalue.getNameWithType() << ";";
        output << candidate.lvalue.getName() << " = &" << lvaluePointee << ";" << std::endl;

        return output.str();
    }


    std::string JsonInputParser::getParserRetrievalForPointersToNonPointers(const JsonParsingCandidate &candidate) {
        if (candidate.lvalue.getType()->getPointerElementType()->isIntegerTy(8)) { // char behavior
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
        handsanitizer::Type *pointeeType = candidate.lvalue.getType()->getPointerElementType();
        std::string elTypeName = pointeeType->getCTypeName();
        std::string jsonValue = candidate.jsonRoot + "[\"value\"]";

        if (!candidate.isForGlobalVariable)
            output << elTypeName << "* " << candidate.lvalue.getName() << ";" << std::endl;


        output << "if(" << jsonValue << ".is_string()) {" << std::endl;

        std::string candidateLvalueAsString = candidate.lvalue.getName();

        //we can't use c_str here because if the string itself contains null, then it it is allowed to misbehave
        output << getStringFromJson(candidateLvalueAsString, jsonValue, this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName()),
                                    this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName()));

        output << "}" << std::endl; // end is_string


        output << "if(" << jsonValue << ".is_number()) {" << std::endl;
        auto tmpLvalueForParsedChar = declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName());
        output << "char " << tmpLvalueForParsedChar << " = " << jsonValue << ".get<char>();" << std::endl;
        output << candidate.lvalue.getName() << " = &" << tmpLvalueForParsedChar << ";" << std::endl;
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
        auto str_len_counter = this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName());
        output << getStringLengthFromJson(str_len_counter, indexedJsonValue, // don't trust .size for nulled strings
                                          this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName()),
                                          this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName()), false);
        output << malloc_size_counter << " =  " << malloc_size_counter << " + " << str_len_counter << ";" << std::endl;
        output << "}" << std::endl; // end is_string


        output << "}" << std::endl; // end for
        // first loop to get the total size
        // then copy

        auto output_buffer_name = this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName());
        auto writeHeadName = this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName()); // writes to this offset in the buffer
        auto indexed_output_buffer_name = output_buffer_name + "[" + writeHeadName + "]";
        output << "char* " << output_buffer_name << " = (char*)malloc(sizeof(char) * " << malloc_size_counter << ");"
               << std::endl;
        output << this->declarationManager->registerVariableToBeFreed(output_buffer_name);
        output << "int " << writeHeadName << " = 0;" << std::endl;
        //here copy, loop again over all and write

        output << "for(int " << nestedIterator << " = 0; " << nestedIterator << " < " << jsonValue << ".size(); "
               << nestedIterator << "++){ " << std::endl;
        output << "if(" << jsonValue << nestedIteratorIndex << ".is_string()){ ";
        auto str_val = this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName());
        output << "std::string " << str_val << " = " << indexedJsonValue << ".get<std::string>();";
        // copy the string into the correct position
        // .copy guarantees to not add an extra null char at the end which is what we desire
        auto charsWritten = this->declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName());
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

        output << candidate.lvalue.getName() << " = " << output_buffer_name << ";"
               << std::endl;
        output << "}" << std::endl; // end is_array

        return output.str();
    }

    std::string JsonInputParser::getParserRetrievalForPointerToNonCharType(const JsonParsingCandidate &candidate) {
        std::stringstream output;

        handsanitizer::Type *pointeeType = candidate.lvalue.getType()->getPointerElementType();

        std::string elTypeName = pointeeType->getCTypeName();
        std::string jsonValue = candidate.jsonRoot + "[\"value\"]";

        std::string argument_name = candidate.lvalue.getName();
        if (!candidate.isForGlobalVariable)
            output << candidate.lvalue.getNameWithType() << ";" << std::endl;

        if (pointeeType->isStructTy()) {
            auto cmp = [&candidate](const std::pair<Type*, std::string>& pair){ return candidate.lvalue.getType()->getPointerElementType() == pair.first;};
            auto helperFunc = std::find_if(structParsingHelperFunctions.begin(), structParsingHelperFunctions.end(), cmp);
            if(helperFunc == structParsingHelperFunctions.end())
                throw std::invalid_argument("Trying to parse a structure for which no helper function exists");


            auto funcName = helperFunc->second;
            output << candidate.lvalue.getName() << " = " << funcName << "(" << candidate.jsonRoot << ");" << std::endl;
        }
        else {
            /*
             * we malloc room and put the freshly parsed value inside it
             * however if we don't know the type we have to declare the pointer as
             */

            output << "if(" << jsonValue << ".is_array()) { " << std::endl;
            std::string malloced_value = declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName() + "_malloced");
            auto malloced_tmp_value = NamedVariable(malloced_value, candidate.lvalue.getType());
            std::string arrSize = candidate.jsonRoot + "[\"value\"].size()";

            // we get a pointer to an object large enough to hold the size of the object
            output << malloced_tmp_value.getNameWithType() <<  " = (" << malloced_tmp_value.getType()->getCTypeName() << ") malloc(sizeof(" << elTypeName << ") * " << arrSize << ");"
                   << std::endl;
            output << declarationManager->registerVariableToBeFreed(malloced_value);

            std::string iterator_name = declarationManager->getUniqueLoopIteratorName();
            output << "for(int " << iterator_name << " = 0; " << iterator_name << " < " << arrSize << ";" << iterator_name << "++) {" << std::endl;

            auto parsingCandidateForLoop = JsonParsingCandidate{
                .lvalue = NamedVariable(std::string(malloced_value + "[" + iterator_name + "]"), pointeeType),
                .jsonRoot = std::string(jsonValue + "[" + iterator_name + "]"),
                .isForGlobalVariable = true // the variable is declared outside of the calling scope
            };
            output << getParserRetrievalForNamedType(parsingCandidateForLoop);
//            output << malloced_value << "[" << iterator_name << "] = " << jsonValue << "[" << iterator_name << "].get<" << elTypeName << ">();" << std::endl;
            output << "}" << std::endl; //endfor

            output << argument_name << " = " << malloced_value << ";" << std::endl;
            output << "}" << std::endl; //end if_array

            // if its a number
            if(!pointeeType->isArrayTy()){
                output << "if(" << jsonValue << ".is_number()){" << std::endl;
                //cast rvalue in case its a char
                auto malloc_name = NamedVariable(declarationManager->getUniqueTmpCPPVariableNameFor(candidate.lvalue.getName()), candidate.lvalue.getType());
                output << malloc_name.getNameWithType() << " = (" << malloc_name.getType()->getCTypeName() << ") malloc(sizeof(" << elTypeName << "));" << std::endl;
                output << this->declarationManager->registerVariableToBeFreed(malloc_name.getName());
    //            output << malloc_name << "[0] = (" << elTypeName << ")" << jsonValue << ".get<" << elTypeName << ">();" << std::endl;

                auto parsingCandidateForNumber = JsonParsingCandidate{
                    .lvalue = NamedVariable(std::string(malloc_name.getName() + "[0]"), pointeeType),
                    .jsonRoot = candidate.jsonRoot,
                    .isForGlobalVariable = true // the variable is declared outside of the calling scope
                };
                output << getParserRetrievalForNamedType(parsingCandidateForNumber);
                output << candidate.lvalue.getName() << " = " << malloc_name.getName() << ";" << std::endl;
                output << "}" << std::endl; //end if number
            }
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



        output << "if(" << jsonRoot << "[\"value\"].is_null()) { return nullptr; } else {" << std::endl;

        auto localLvalue = declarationManager->getUniqueTmpCPPVariableNameFor();

        output << type->getCTypeName() << "* " << localLvalue << " = (" << type->getCTypeName() << "*) malloc(sizeof(" << type->getCTypeName() << "));" << std::endl;
        auto pointeeCandidate = JsonParsingCandidate{
            .lvalue = NamedVariable(localLvalue, type),
            .jsonRoot = jsonRoot,
            .isForGlobalVariable = false
        };
        if(!pointeeCandidate.lvalue.getType()->isStructTy())
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

