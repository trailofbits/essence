//
// Created by sabastiaan on 22-07-21.
//
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <iomanip>
#include "Module.h"
#include "include/name_generation.hpp"
#include "include/code_generation.hpp"

namespace handsanitizer{
void Module::generate_cpp_file_for_function(Function &f, std::string dest_file_path) {
    this->definedNamesForFunctionBeingGenerated.clear();
    std::stringstream setupfilestream;

    setupfilestream << "#include \"skelmain.hpp\" " << std::endl;
    setupfilestream << "#include <cstdint>" << std::endl;
    setupfilestream << "#include <iomanip>" << std::endl;

    setupfilestream << "#include <nlohmann/json.hpp> // header only lib" << std::endl << std::endl;


    // DECLARE STRUCTS

    setupfilestream << getTextForUserDefinedTypes();

    // DECLARATION TEST FUNCTION
    // the LLVM type name differs from how we define it in our generated source code
    std::string functionSignature = f.retType->getCTypeName() + " " + f.name + "(" + getTypedArgumentNames(f) + ");";
    setupfilestream << "extern \"C\" " + functionSignature << std::endl;

    // DECLARE GLOBALS
    setupfilestream << "argparse::ArgumentParser parser;" << std::endl << std::endl;
    setupfilestream << "// globals from module" << std::endl << getGlobalDeclarationsText() << std::endl << std::endl;


    // DEFINE SETUPPARSER
    setupfilestream << "void setupParser() { " << std::endl
                    << "std::stringstream s;"
                    << "s << \"Test program for: " << functionSignature << "\" << std::endl << R\"\"\"("  << getTextForUserDefinedTypes() << ")\"\"\";" << std::endl
                    << "\tparser = argparse::ArgumentParser(s.str());" << std::endl
                    << "} " << std::endl << std::endl;

    // DEFINE CALLFUNCTION
    setupfilestream << "void callFunction() { " << std::endl
                    << "\t\t" << "std::string inputfile = parser.get<std::string>(\"-i\");" << std::endl;
    auto inputFileName = getUniqueTmpCPPVariableNameFor("inputFile");
    auto jsonInputVariable = getUniqueTmpCPPVariableNameFor("j");
                    setupfilestream << "\t\t" << "std::ifstream " <<  inputFileName << "(inputfile);\n"
                                 "\t\tnlohmann::json " << jsonInputVariable << ";" << std::endl
                                 << inputFileName
                                 << " >> "
                                 << jsonInputVariable << ";" << std::endl
                    << getParserRetrievalTextForGlobals(jsonInputVariable)
                    << getParserRetrievalTextForArguments(jsonInputVariable, f.arguments);

    auto output_var_name = getUniqueTmpCPPVariableNameFor("output");
    if(f.retType->isVoidTy() == false)
        setupfilestream << "auto " << output_var_name << " = ";
    setupfilestream <<  f.name << "(" << getUntypedArgumentNames(f) << ");" << std::endl;
    setupfilestream <<  getJsonOutputText(output_var_name, f.retType);




    setupfilestream << "} " << std::endl;
    std::string setupFileString = setupfilestream.str();

    std::ofstream of;
    of.open(dest_file_path, std::ofstream::out | std::ofstream::trunc);
    of << setupFileString;
    of.close();
}


std::string Module::getTextForUserDefinedTypes() {
    std::stringstream output;
    for(auto& custom_type : this->user_defined_types){
        output << Module::getTextForUserDefinedType(custom_type);
    }
    return output.str();
}

// assumes all dependacies are defined before hand
std::string Module::getTextForUserDefinedType(Type * type) {
    std::stringstream output;
    output << "typedef struct " << type->getCTypeName() << " {" << std::endl;
    for(auto& member: type->getNamedMembers()){
        if(member.getType()->isArrayTy()){
            output << member.getType()->getArrayElementType()->getCTypeName() << " " << member.getName();
            output << "[" << member.getType()->getArrayNumElements() << "]";
        }
        else{
            output << member.getType()->getCTypeName() << " " <<  member.getName();
        }
        output << ";" << std::endl;
    }
    output << "} " << type->getCTypeName() << ";" << std::endl;
    return output.str();
}

std::string Module::getTypedArgumentNames(Function &f) {
    std::stringstream output;
    for(auto& args : f.arguments){
        output << args.getType()->getCTypeName() << " " << args.getName() << ",";
    }
    auto retstring = output.str();
    if(!retstring.empty())
        retstring.pop_back(); //remove last ,

    return retstring;
}

std::string Module::getUntypedArgumentNames(Function &f) {
    std::stringstream output;
    for(auto& args : f.arguments){
        output << args.getName() << ",";
    }
    auto retstring = output.str();
    if(!retstring.empty())
        retstring.pop_back(); //remove last ,

    return retstring;
}


std::string Module::getGlobalDeclarationsText() {
    std::stringstream s;
    for(auto& global : this->globals){
        s << "extern " << global.getType()->getCTypeName()  << " " << global.getName() << ";" << std::endl;
    }
    return s.str();
}

std::string Module::getParserRetrievalTextForGlobals(std::string jsonInputVariableName) {
    std::vector<NamedVariable> globals(this->globals.begin(), this->globals.end());
    jsonInputVariableName = jsonInputVariableName + "[\"globals\"]";

    return getParserRetrievalText(jsonInputVariableName, globals, true);
}

std::string Module::getParserRetrievalTextForArguments(std::string jsonInputVariableName, std::vector<Argument> args) {
    std::vector<NamedVariable> a(args.begin(), args.end());
// TODO: Woudl this work?
//        return getParserRetrievalText((std::vector<NamedVariable>&)args, false);
    jsonInputVariableName = jsonInputVariableName + "[\"arguments\"]";
    return getParserRetrievalText(jsonInputVariableName, a, false);
}


std::string Module::getParserRetrievalText(std::string jsonInputVariableName, std::vector<NamedVariable> args, bool isForGlobals) {
    std::stringstream s;

    for(auto& a : args){
        std::vector<std::string> dummy;
        dummy.push_back(a.getName());

        // llvm boxes structs always with a pointer?
        if(a.type->isPointerTy() && a.type->getPointerElementType()->isStructTy())
            s << getParserRetrievalForNamedType(jsonInputVariableName, dummy, a.type->getPointerElementType(), isForGlobals);
        else{
            s << getParserRetrievalForNamedType(jsonInputVariableName, dummy, a.type, isForGlobals);
        }
    }
    return s.str();
}

std::string Module::getParserRetrievalForNamedType(std::string jsonInputVariableName, std::vector<std::string> prefixes, handsanitizer::Type *type,
                                                   bool isForGlobals) {
    std::stringstream output;

    // arrays only exists in types unless they are global
    if(isForGlobals && type->isArrayTy()){
        int arrSize = type->getArrayNumElements();
        handsanitizer::Type* arrType = type->getArrayElementType();
        std::string iterator_name = getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
        output << "for(int " << iterator_name << " = 0; " << iterator_name << " < "<< arrSize << ";" << iterator_name << "++) {" << std::endl;
        std::vector<std::string> fullname(prefixes);
        fullname.push_back(iterator_name);
        output << getParserRetrievalForNamedType(jsonInputVariableName, fullname, arrType, false); // we declare a new local variable so don't pass in the global flag
        output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << "[" << iterator_name << "] = " << joinStrings(fullname, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;
        output << "}" << std::endl;
    }


        // Pointers recurse until they point to a non pointer
        // for a non-pointer it declares the base type and sets it as an array/scalar
    else if(type->isPointerTy()) {
        if (type->getPointerElementType()->isPointerTy()) {
            std::vector<std::string> referenced_name(prefixes);
            referenced_name.push_back(POINTER_DENOTATION);
            output << getParserRetrievalForNamedType(jsonInputVariableName, referenced_name, type->getPointerElementType(), isForGlobals);

            output << "\t\t";
            if (!isForGlobals)
                output << type->getCTypeName() << " ";
            output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = &"
                   << joinStrings(referenced_name, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;
        }
        else { // we know ptr points to a non pointer
            handsanitizer::Type* pointeeType = type->getPointerElementType();
            std::string elTypeName = pointeeType->getCTypeName();
            std::string arrSize =  jsonInputVariableName + joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING) + ".size()";
            std::string jsonValue = jsonInputVariableName + joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING);
            //declare var, as we recurse it must always be an extra definition
            output << elTypeName << "* " << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;


            // normal behavior
            if (!pointeeType->isIntegerTy(8)){
                // if its an array
                output << "if(" << jsonValue << ".is_array()) { " << std::endl;
                std::vector<std::string> malloced_value(prefixes);
                malloced_value.push_back("malloced");


                output << elTypeName << "* " << joinStrings(malloced_value, GENERATE_FORMAT_CPP_VARIABLE);

                // TODO: should add a corresponding free
                output << " = (" << elTypeName << "*) malloc(sizeof(" << elTypeName << ") * " << arrSize << ");" << std::endl;

                std::string iterator_name = getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
                output << "for(int " << iterator_name << " = 0; " << iterator_name << " < "<< arrSize << ";" << iterator_name << "++) {" << std::endl;
                output << joinStrings(malloced_value, GENERATE_FORMAT_CPP_VARIABLE) << "[" << iterator_name << "] = j" << joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING) << "[" << iterator_name << "].get<" << elTypeName << ">();" << std::endl;
                output << "}" << std::endl; //endfor

                output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = " << joinStrings(malloced_value, GENERATE_FORMAT_CPP_VARIABLE) << ";" <<std::endl;
                output << "}" << std::endl; //end if_array

                // if its a number
                output << "if(" << jsonValue << ".is_number()){" << std::endl;
                //cast rvalue in case its a char
                auto stack_alloced_name = joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) + "_stack_alloced";
                output << elTypeName << " " <<  stack_alloced_name << "= (" << elTypeName << ")" << jsonValue << ".get<" << elTypeName << ">();" << std::endl;
                output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = &" << stack_alloced_name << ";" << std::endl;
                output << "}" << std::endl; //end if number

            }
            else{ // char behavior
                /*
                 * we handle chars in 3 different ways, l and rvalues here are in hson
                 * char_value = "string" => null terminated
                 * char_value = 70 => ascii without null
                 * char_value = ["f", "o", NULL , "o"] => in memory this should reflect f o NULL o, WITHOUT EXPLICIT NULL TERMINATION!
                 * char_value = ["fooo", 45, NULL, "long string"], same rules apply here as above, this should not have a null all the way at the end
                 */


                output << "if(" << jsonValue << ".is_string()) {" << std::endl;

                std::string string_tmp_val = this->getUniqueTmpCPPVariableNameFor(prefixes);
//                output << "std::string " << joinStrings(string_tmp_val, GENERATE_FORMAT_CPP_VARIABLE) << " = " << jsonValue << ".get<std::string>();" << std::endl;

                //we can't use c_str here because if the string itself contains null, then it it is allowed to misbehave
                output << getStringFromJson(string_tmp_val, jsonValue, this->getUniqueTmpCPPVariableNameFor(prefixes),
                                            this->getUniqueTmpCPPVariableNameFor(prefixes));

                output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = " << string_tmp_val << ";" << std::endl;
                output << "}" << std::endl; // end is_string


                output << "if(" << jsonValue << ".is_number()) {" << std::endl;
                auto tmp_char_val = this->getUniqueTmpCPPVariableNameFor(prefixes);
                output << "char " << tmp_char_val << " = " << jsonValue << ".get<char>();" << std::endl; // TODO: no idea if it accepts .get<char>
                output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = &" << tmp_char_val << ";" << std::endl;
                output << "}" << std::endl; //end is_number


                output << "if(" << jsonValue << ".is_array()) {" << std::endl;
                auto nestedIterator =  getUniqueLoopIteratorName();
                auto nestedIteratorIndex ="[" + nestedIterator + "]";
                auto indexedJsonValue = jsonValue + nestedIteratorIndex;
                output << "int malloc_size_counter = 0;" << std::endl;
                output << "for(int " << nestedIterator << " = 0; " << nestedIterator << " < " << jsonValue << ".size(); " << nestedIterator << "++){ " << std::endl;
                output << "if(" << indexedJsonValue << ".is_number() || " << jsonValue << nestedIteratorIndex << ".is_null()) malloc_size_counter++;"; // assume its a char
                output << "if(" << indexedJsonValue << ".is_string()){"; // assume its a byte
                auto str_len_counter = this->getUniqueTmpCPPVariableNameFor(prefixes);
                output << getStringLengthFromJson(str_len_counter, indexedJsonValue, // don't trust .size for nulled strings
                                                  this->getUniqueTmpCPPVariableNameFor(prefixes), this->getUniqueTmpCPPVariableNameFor(prefixes), false);
                output << "malloc_size_counter = malloc_size_counter + " << str_len_counter << ";" << std::endl;
                output << "}" << std::endl; // end is_string


                output << "}" << std::endl; // end for
                // first loop to get the total size
                // then copy

                auto output_buffer_name = this->getUniqueTmpCPPVariableNameFor(prefixes);
                auto writeHeadName = this->getUniqueTmpCPPVariableNameFor(prefixes); // writes to this offset in the buffer
                auto indexed_output_buffer_name = output_buffer_name + "[" + writeHeadName + "]";
                output << "char* " << output_buffer_name << " = (char*)malloc(sizeof(char) * malloc_size_counter);" << std::endl;
                output << "int " << writeHeadName << " = 0;" << std::endl;
                //here copy, loop again over all and write

                output << "for(int " << nestedIterator << " = 0; " << nestedIterator << " < " << jsonValue << ".size(); " << nestedIterator << "++){ " << std::endl;
                output << "if(" << jsonValue << nestedIteratorIndex << ".is_string()){ ";
                auto str_val = this->getUniqueTmpCPPVariableNameFor(prefixes);
                output << "std::string " << str_val << " = " << indexedJsonValue << ".get<std::string>();";
                // copy the string into the correct position
                // .copy guarantees to not add an extra null char at the end which is what we desire
                auto charsWritten  = this->getUniqueTmpCPPVariableNameFor(prefixes);
                // TODO: Check if this indeed copys part way into the buffer
                output << "auto " << charsWritten << " = " << str_val << ".copy(&" << indexed_output_buffer_name << ", " << str_val << ".size(), " << "0);" << std::endl;
                output << writeHeadName << " = " << writeHeadName << " + " << charsWritten << ";" << std::endl;
                output << "}" << std::endl; // end is string

                output << "if(" << jsonValue << nestedIteratorIndex << ".is_null()){ ";
                output << indexed_output_buffer_name << " = '\\x00';" << std::endl;
                output << writeHeadName << "++;" << std::endl;
                output << "}" << std::endl; // end is string

                output << "if(" << jsonValue << nestedIteratorIndex << ".is_number()){ ";

                //TODO: Maybe add some error handling here
                output << indexed_output_buffer_name << " = (char)" << indexedJsonValue << ".get<int>();" << std::endl;
                output << writeHeadName << "++;" << std::endl;
                output << "}" << std::endl; // end is string


                // if a string copy for length?

                output << "}" << std::endl; // end for

                output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = " << output_buffer_name << ";" << std::endl; // TODO: no idea if it accepts .get<char>
                output << "}" << std::endl; // end is_array
            }



            // char array stuff
        }
    }


    // if type is scalar => output directly
    if(type->isIntegerTy(1) || type->isIntegerTy(8) || type->isIntegerTy(16) || type->isIntegerTy(32) || type->isIntegerTy(64)){
        output << "\t\t";
        std::string parserArg = joinStrings(prefixes, GENERATE_FORMAT_CPP_ADDRESSING);

        if(!isForGlobals) // only declare types if its not global, otherwise we introduce them as local variable
            output << type->getCTypeName() << " ";
        else {

            parserArg = "global." + parserArg;
        }
        //declare lvalue
        output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE);
        output << " = " << jsonInputVariableName << joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING) << ".get<" << type->getCTypeName() << ">();";
        output << std::endl;
    }

    // if type is struct, recurse with member names added as prefix
    if(type->isStructTy()){
        // first do non array members
        // declare and assign values to struct
        // then fill in struct arrays (as these can only be filled in post declaration)

        for(auto member : type->getNamedMembers()){
            if(member.getType()->isArrayTy() == false){
                std::vector<std::string> fullMemberName(prefixes);
                fullMemberName.push_back(member.getName());
                output << getParserRetrievalForNamedType(jsonInputVariableName,fullMemberName, member.getType(), false);
            }
        }
        output << "\t";
        if(!isForGlobals)
            output << type->getCTypeName() << " ";
        output  << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE);
        if(isForGlobals)
            output << " = { " << std::endl; // with globals we have different syntax :?
        else
            output << "{ " << std::endl;
        for(auto member : type->getNamedMembers()){
            if(member.getType()->isArrayTy() == false){
                std::vector<std::string> fullMemberName(prefixes);
                fullMemberName.push_back(member.getName());
                //TODO formalize the fullmember name (lvalue name for member)
                output << "\t\t" << "." << member.getName() << " = " << joinStrings(fullMemberName, GENERATE_FORMAT_CPP_VARIABLE) << "," << std::endl;
            }
        }
        output << "\t};" << std::endl;

        for(auto member : type->getNamedMembers()){
            // arrays can be also of custom types with arbitrary but finite amounts of nesting, so we need to recurse
            if(member.getType()->isArrayTy()){
                int arrSize = member.getType()->getArrayNumElements();
                Type* arrType = member.getType()->getArrayElementType();
                std::string iterator_name = getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
                output << "for(int " << iterator_name << " = 0; " << iterator_name << " < "<< arrSize << ";" << iterator_name << "++) {" << std::endl;
                std::vector<std::string> fullMemberName(prefixes);
                fullMemberName.push_back(member.getName());
                fullMemberName.push_back(iterator_name);
                output << getParserRetrievalForNamedType(jsonInputVariableName, fullMemberName, arrType, false);
                output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << "." << member.getName() << "[" <<  iterator_name << "] = " << joinStrings(fullMemberName, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;
                output << "}" << std::endl;
            }
        }
    }
    return output.str();
}

    std::string Module::getUniqueTmpCPPVariableNameFor(std::vector<std::string> prefixes) {
        auto candidate = joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE);

        // TODO: Change this in a proper RNG call
        int counter = 0;
        while(true){
            if(!this->isNameDefined(candidate)){
                definedNamesForFunctionBeingGenerated.push_back(candidate);
                return candidate;
            }

            candidate = candidate + getRandomDummyVariableName();
            counter++;
        }
    }

    std::vector<std::string> TmpStringCandidates{
            "foo",
            "bar",
            "baz",
            "alice",
            "bob",
            "carlos",
            "dan",
            "erin",
            "eve",
            "judy",
            "levi",
            "micheal",
            "panda", // hi my name is
            "slim",
            "shady",
            "vera",
            "victor",
            "ted",
    };

    std::string Module::getRandomDummyVariableName() {
        return TmpStringCandidates[rand() % TmpStringCandidates.size()];
    }

    std::string Module::getUniqueTmpCPPVariableNameFor() {
        std::vector<std::string> vec;
        vec.push_back(getRandomDummyVariableName());
        return getUniqueTmpCPPVariableNameFor(vec);
    }

    bool Module::isNameDefined(std::string name) {
        return std::find(std::begin(this->definedNamesForFunctionBeingGenerated), std::end(this->definedNamesForFunctionBeingGenerated), name) != std::end(this->definedNamesForFunctionBeingGenerated);
    }

    std::string Module::getUniqueTmpCPPVariableNameFor(std::string prefix) {
        std::vector<std::string> vec;
        vec.push_back(prefix);
        return getUniqueTmpCPPVariableNameFor(vec);
    }

    std::string getJsonInputTemplateTextForJsonRvalue(Type& arg){
        std::stringstream output;
        if(arg.isPointerTy())
            output << getJsonInputTemplateTextForJsonRvalue(*arg.getPointerElementType()); // we abstract pointers away

        // if scalar
        if(arg.isArrayTy() == false && arg.isStructTy() == false && !arg.isPointerTy()){
            output << arg.getCTypeName();
        }

        if(arg.isArrayTy()){
            output << "[";
            for(int i = 0; i < arg.getArrayNumElements(); i++){
                output << getJsonInputTemplateTextForJsonRvalue(*arg.getArrayElementType());
                if (i != arg.getArrayNumElements() -1){
                    output << ",";
                }
            }
            output << "]";
        }

        if(arg.isStructTy()){
            output << "{";
            for(auto& mem : arg.getNamedMembers()){
                output << "\"" << mem.getName() << "\"" << ": " << getJsonInputTemplateTextForJsonRvalue(*mem.getType());
                if (mem.getName() != arg.getNamedMembers().back().getName()){ // names should be unique
                    output << "," << std::endl;
                }
            }
            output << "}";
        }
        return output.str();
    }

    void Module::generate_json_input_template_file(Function &f, std::string dest_file_path) {
        std::ofstream of ;
        of.open(dest_file_path, std::ofstream::out | std::ofstream::trunc);

        std::stringstream output;
        output << "{" << std::endl;
        output << "\"globals\": {" << std::endl;
            for(auto& arg: this->globals){
                output << "\"" << arg.getName() << "\"" << ": " << getJsonInputTemplateTextForJsonRvalue(*arg.getType());
                if (arg.getName() != f.arguments.back().getName()){
                    output << "," << std::endl;
                }
            }

        output << "}," << std::endl;

        output << "\"arguments\": {" << std::endl;
        for(auto& arg: f.arguments){
            output << "\"" << arg.getName() << "\"" << ": " << getJsonInputTemplateTextForJsonRvalue(*arg.getType());
            if (arg.getName() != f.arguments.back().getName()){
                output << "," << std::endl;
            }
        }
        output << "}" << std::endl;

        output << "}" << std::endl;
        of << std::setw(4) << output.str() << std::endl;
        of.close();
    }


    std::string Module::getJsonOutputForType(std::string json_name, std::vector<std::string> prefixes, Type* type){
        std::stringstream s;

        if(type->isPointerTy()){
//            //TODO: Should this be casted to anything to ensure proper output format?
//            // should do this for the longest pointer type
//            s << "if(sizeof(" << type->getCTypeName() << ") == 4) " << std::endl;
//            s << json_name << joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING)
//              << " = (uint32_t)" << joinStrings(prefixes, GENERATE_FORMAT_CPP_ADDRESSING) << ";" << std::endl;

//            s << "if(sizeof(" << type->getCTypeName() << ") == 8) " << std::endl;
            s << json_name << joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING)
              << " = (uint64_t)" << joinStrings(prefixes, GENERATE_FORMAT_CPP_ADDRESSING) << ";" << std::endl;
        }
        else if(type->isStructTy()){
            for(auto& mem : type->getNamedMembers()){
                std::vector<std::string> member_prefixes;
                member_prefixes.push_back(mem.getName());
                s << getJsonOutputForType(json_name, member_prefixes, mem.getType()) << std::endl;
            }
        }
        else{
            s << json_name << joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING)
              << " = " << joinStrings(prefixes, GENERATE_FORMAT_CPP_ADDRESSING) << ";" << std::endl;
        }
        return s.str();
    }


// assume output_var_name contains the return value already set
    std::string Module::getJsonOutputText(std::string output_var_name, handsanitizer::Type* retType){
        std::stringstream s;
        auto jsonVarName = getUniqueTmpCPPVariableNameFor("output_json");
        s << "nlohmann::json " <<  jsonVarName  << ";" << std::endl;
        std::vector<std::string> prefixes;
        for(auto& global : this->globals){
            prefixes.clear();
            prefixes.push_back(global.getName());
            s << getJsonOutputForType(jsonVarName + "[\"globals\"]", prefixes, global.getType());
        }

        prefixes.clear();
        prefixes.push_back(output_var_name);
        if(retType->isVoidTy() == false)
            s << getJsonOutputForType(jsonVarName, prefixes, retType);



        s << "std::cout << std::setw(4) << " << jsonVarName << " << std::endl;" << std::endl;
        return s.str();
    }

    void Module::generate_json_module_specification(std::string dest_file_path) {
        std::ofstream of;
        of.open(dest_file_path, std::ofstream::out | std::ofstream::trunc);

        std::stringstream output;
        output << "{" << std::endl;
        output << "bitcode_module: \"" << this->name << "\","  << std::endl;
        output << "globals: {" << std::endl;
        for(auto& g : this->globals){
            output << "\"" << g.getName() << "\"" << ": " << getUnrolledTypeAsJson(*g.getType());
            if(g.getName() != this->globals.back().name)
                output << ",";
        }

        output << "}," << std::endl;
        output << "functions: {" << std::endl;
        for(auto& f: this->functions){
            output << "name: \"" <<  f.name << "\"," << std::endl;
            output << "arguments: {" << std::endl;
            for(auto& a : f.arguments){
                output << "\"" << a.getName() << "\"" << ": " << getUnrolledTypeAsJson(*a.getType());
                if(a.getName() != f.arguments.back().name)
                    output << ",";
            }

            if(f.name != this->functions.back().name)
                output << ",";
            output << "}" << std::endl;
        }

        output << "}" << std::endl;
        output << "}" << std::endl;


        of << std::setw(4) <<output.str();
        of.close();
    }

    std::string Module::getUnrolledTypeAsJson(Type& type) {
        std::stringstream output;
        output << "{" << std::endl;
        output << "type: \"" << type.getTypeName() << "\"";
        if(type.isVoidTy()){
            // do nothing
        }

        if(type.isPointerTy())
            output << ", pointerElementType: " << getUnrolledTypeAsJson(*type.getPointerElementType());

        if(type.isIntegerTy())
            output << ", bitWidth: " << type.getBitWidth();

        if(type.isStructTy()){
            output << ", structName: " << type.getCTypeName() << ", structMembers: [" ;
            for(auto& member : type.getNamedMembers()){
                output << "\"" << member.getName() << "\"" << ": " << getUnrolledTypeAsJson(*member.getType());
                if(member.getName() != type.getNamedMembers().back().name)
                    output << ",";
            }
            output << "]";
        }
        if(type.isArrayTy()){
            output << ", getArrayNumElements: " << type.getArrayNumElements() << ", arrayElementType:" << getUnrolledTypeAsJson(*type.getArrayElementType());
        }

        output << "}" << std::endl;

        return output.str();
    }

}