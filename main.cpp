//
// Created by sabastiaan on 22-06-21.
//
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/AssemblyAnnotationWriter.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"


#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/WithColor.h"
#include <system_error>
#include <iostream>
#include <sstream>
#include <fstream>
#include "llvm/IR/LegacyPassManager.h"



#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

#include "llvm/Transforms/Utils.h"
#include <filesystem>

#include "include/name_generation.hpp"
#include "include/code_generation.hpp"
#include "include/handsan.hpp"
#include "LLVMExtractor.hpp"


static llvm::ExitOnError ExitOnErr;

static llvm::cl::opt<std::string>
        InputFilename(llvm::cl::Positional, llvm::cl::desc("<input bitcode>"), llvm::cl::init("-"));


// if we encounter a struct or union type we must define it first in our program
// we do this through a DFS on the elements of every argument we find
// as long as the result of every DFS search on another root/non-recursive call is appended AFTER the previous result
// we won't get into a nasty dependency problem. This follows trivially from that at each point
// we either generate direct dependencies or they are already defined before hand.
// To not re-define previously defined structs we save the name in an vector
//std::string getSetupFileText(std::string functionName, handsanitizer::Type *funcRetType, std::vector<handsanitizer::Argument> &args_of_func,
//                             std::vector<handsanitizer::GlobalVariable> &globals);
//
//std::vector<handsanitizer::Argument> extractArgumentsFromFunction(llvm::Function &f);
//
//void GenerateJsonInputTemplateFile(const std::string &funcName, std::vector<handsanitizer::Argument> &args_of_func,
//                                   std::vector<handsanitizer::GlobalVariable> &globals);
//
//std::vector<handsanitizer::GlobalVariable> extractGlobalValuesFromModule(std::unique_ptr<llvm::Module> &mod);
//
//void GenerateCppFunctionHarness(std::string &funcName, handsanitizer::Type *funcRetType, std::vector<handsanitizer::Argument> &args_of_func,
//                                std::vector<handsanitizer::GlobalVariable> &globals_of_func);
//
//
//std::string getDefineGlobalsText(std::vector<handsanitizer::GlobalVariable> globals){
//    int a = getNum();
//    std::stringstream s;
//    for(auto& global : globals){
//        s << "extern " << getCTypeNameForLLVMType(global.getType()) << " " << global.getName() << ";" << std::endl;
//    }
//
//    return s.str();
//}
//
//
//std::vector<std::string> used_tmps;
//
//// a string is unique mostly for a string path like this
//std::string getUniqueCppTmpString(std::vector<std::string> prefixes){
//    auto s = joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE);
//    used_tmps.push_back(s);
//    s = s + std::to_string(used_tmps.size()); // really shitty implementation but  works for now
//
//    return s;
//}
//
//std::string getStructDefinitions(){
//    return getRegisteredStructs();
//}

// Names are constructed the following
// Consider an actual argumetn to the tested function, and call their name root
// For any scalar type we just print the name
// If its a struct all member names need to following convention
// <type> assignment_name = parser.get<type>(parsername)
// where assignment name is delimited with _ and parsername with .
// This way we can keep going deeper in the hierarcy but just adding the latest struct name
// and forfill both conventions

/*
 * assignment_name is always prefixes.joined('_')
 * as assignment name has to deal with multiple pointer levels, we add a pointer depth
 * notice that we cannot encode how many extra variables we have made to deal with pointers as we need to keep prefixes only reflect structure depth
 * otherwise the parser.get("--name") we would diverge with the setup
 */
//std::string getParserRetrievalForNamedType(std::vector<std::string> prefixes, handsanitizer::Type* type, bool isForGlobals = false){
//    std::stringstream output;
//    // arrays only exists in types unless they are global
//    if(isForGlobals && type->isArrayTy()){
//        int arrSize = type->getArrayNumElements();
//        handsanitizer::Type* arrType = type->getArrayElementType();
//        std::string iterator_name = getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
//        output << "for(int " << iterator_name << " = 0; " << iterator_name << " < "<< arrSize << ";" << iterator_name << "++) {" << std::endl;
//        std::vector<std::string> fullname(prefixes);
//        fullname.push_back(iterator_name);
//        output << getParserRetrievalForNamedType(fullname, arrType, false); // we declare a new local variable so don't pass in the global flag
//        output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << "[" << iterator_name << "] = " << joinStrings(fullname, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;
//        output << "}" << std::endl;
//    }
//
//
//    // Pointers recurse until they point to a non pointer
//    // for a non-pointer it declares the base type and sets it as an array/scalar
//    else if(type->isPointerTy()) {
//        if (type->getPointerElementType()->isPointerTy()) {
//            std::vector<std::string> referenced_name(prefixes);
//            referenced_name.push_back(POINTER_DENOTATION);
//            output << getParserRetrievalForNamedType(referenced_name, type->getPointerElementType(), isForGlobals);
//
//            output << "\t\t";
//            if (!isForGlobals)
//                output << getCTypeNameForLLVMType(type) << " ";
//            output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = &"
//                   << joinStrings(referenced_name, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;
//        }
//        else { // we know ptr points to a non pointer
//            handsanitizer::Type* pointeeType = type->getPointerElementType();
//            std::string elTypeName = getCTypeNameForLLVMType(pointeeType);
//            std::string arrSize =  "j" + joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING) + ".size()";
//            std::string jsonValue = "j" + joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING);
//            //declare var, as we recurse it must always be an extra definition
//            output << elTypeName << "* " << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;
//
//
//            // normal behavior
//            if (!pointeeType->isIntegerTy(8)){
//                // if its an array
//                output << "if(" << jsonValue << ".is_array()) { " << std::endl;
//                std::vector<std::string> malloced_value(prefixes);
//                malloced_value.push_back("malloced");
//
//
//                output << elTypeName << "* " << joinStrings(malloced_value, GENERATE_FORMAT_CPP_VARIABLE);
//
//                // TODO: should add a corresponding free
//                output << " = (" << elTypeName << "*) malloc(sizeof(" << elTypeName << ") * " << arrSize << ");" << std::endl;
//
//                std::string iterator_name = getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
//                output << "for(int " << iterator_name << " = 0; " << iterator_name << " < "<< arrSize << ";" << iterator_name << "++) {" << std::endl;
//                output << joinStrings(malloced_value, GENERATE_FORMAT_CPP_VARIABLE) << "[" << iterator_name << "] = j" << joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING) << "[" << iterator_name << "].get<" << elTypeName << ">();" << std::endl;
//                output << "}" << std::endl; //endfor
//
//                output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = " << joinStrings(malloced_value, GENERATE_FORMAT_CPP_VARIABLE) << ";" <<std::endl;
//                output << "}" << std::endl; //end if_array
//
//                // if its a number
//                output << "if(" << jsonValue << ".is_number()){" << std::endl;
//                //cast rvalue in case its a char
//                auto stack_alloced_name = joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) + "_stack_alloced";
//                output << elTypeName << " " <<  stack_alloced_name << "= (" << elTypeName << ")" << jsonValue << ".get<" << elTypeName << ">();" << std::endl;
//                output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = &" << stack_alloced_name << ";" << std::endl;
//                output << "}" << std::endl; //end if number
//
//            }
//            else{ // char behavior
//                /*
//                 * we handle chars in 3 different ways, l and rvalues here are in hson
//                 * char_value = "string" => null terminated
//                 * char_value = 70 => ascii without null
//                 * char_value = ["f", "o", NULL , "o"] => in memory this should reflect f o NULL o, WITHOUT EXPLICIT NULL TERMINATION!
//                 * char_value = ["fooo", 45, NULL, "long string"], same rules apply here as above, this should not have a null all the way at the end
//                 */
//
//
//                output << "if(" << jsonValue << ".is_string()) {" << std::endl;
//
//                std::string string_tmp_val = getUniqueCppTmpString(prefixes);
////                output << "std::string " << joinStrings(string_tmp_val, GENERATE_FORMAT_CPP_VARIABLE) << " = " << jsonValue << ".get<std::string>();" << std::endl;
//
//                //we can't use c_str here because if the string itself contains null, then it it is allowed to misbehave
//                output << getStringFromJson(string_tmp_val, jsonValue, getUniqueCppTmpString(prefixes),
//                                            getUniqueCppTmpString(prefixes));
//
//                output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = " << string_tmp_val << ";" << std::endl;
//                output << "}" << std::endl; // end is_string
//
//
//                output << "if(" << jsonValue << ".is_number()) {" << std::endl;
//                auto tmp_char_val = getUniqueCppTmpString(prefixes);
//                output << "char " << tmp_char_val << " = " << jsonValue << ".get<char>();" << std::endl; // TODO: no idea if it accepts .get<char>
//                output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = &" << tmp_char_val << ";" << std::endl;
//                output << "}" << std::endl; //end is_number
//
//
//                output << "if(" << jsonValue << ".is_array()) {" << std::endl;
//                auto nestedIterator =  getUniqueLoopIteratorName();
//                auto nestedIteratorIndex ="[" + nestedIterator + "]";
//                auto indexedJsonValue = jsonValue + nestedIteratorIndex;
//                output << "int malloc_size_counter = 0;" << std::endl;
//                output << "for(int " << nestedIterator << " = 0; " << nestedIterator << " < " << jsonValue << ".size(); " << nestedIterator << "++){ " << std::endl;
//                output << "if(" << indexedJsonValue << ".is_number() || " << jsonValue << nestedIteratorIndex << ".is_null()) malloc_size_counter++;"; // assume its a char
//                output << "if(" << indexedJsonValue << ".is_string()){"; // assume its a byte
//                auto str_len_counter = getUniqueCppTmpString(prefixes);
//                output << getStringLengthFromJson(str_len_counter, indexedJsonValue, // don't trust .size for nulled strings
//                                                  getUniqueCppTmpString(prefixes), getUniqueCppTmpString(prefixes), false);
//                output << "malloc_size_counter = malloc_size_counter + " << str_len_counter << ";" << std::endl;
//                output << "}" << std::endl; // end is_string
//
//
//                output << "}" << std::endl; // end for
//                // first loop to get the total size
//                // then copy
//
//                auto output_buffer_name = getUniqueCppTmpString(prefixes);
//                auto writeHeadName = getUniqueCppTmpString(prefixes); // writes to this offset in the buffer
//                auto indexed_output_buffer_name = output_buffer_name + "[" + writeHeadName + "]";
//                output << "char* " << output_buffer_name << " = (char*)malloc(sizeof(char) * malloc_size_counter);" << std::endl;
//                output << "int " << writeHeadName << " = 0;" << std::endl;
//                //here copy, loop again over all and write
//
//                output << "for(int " << nestedIterator << " = 0; " << nestedIterator << " < " << jsonValue << ".size(); " << nestedIterator << "++){ " << std::endl;
//                output << "if(" << jsonValue << nestedIteratorIndex << ".is_string()){ ";
//                    auto str_val = getUniqueCppTmpString(prefixes);
//                    output << "std::string " << str_val << " = " << indexedJsonValue << ".get<std::string>();";
//                    // copy the string into the correct position
//                    // .copy guarantees to not add an extra null char at the end which is what we desire
//                    auto charsWritten  = getUniqueCppTmpString(prefixes);
//                    // TODO: Check if this indeed copys part way into the buffer
//                    output << "auto " << charsWritten << " = " << str_val << ".copy(&" << indexed_output_buffer_name << ", " << str_val << ".size(), " << "0);" << std::endl;
//                    output << writeHeadName << " = " << writeHeadName << " + " << charsWritten << ";" << std::endl;
//                output << "}" << std::endl; // end is string
//
//                output << "if(" << jsonValue << nestedIteratorIndex << ".is_null()){ ";
//                output << indexed_output_buffer_name << " = '\\x00';" << std::endl;
//                output << writeHeadName << "++;" << std::endl;
//                output << "}" << std::endl; // end is string
//
//                output << "if(" << jsonValue << nestedIteratorIndex << ".is_number()){ ";
//
//                //TODO: Maybe add some error handling here
//                output << indexed_output_buffer_name << " = (char)" << indexedJsonValue << ".get<int>();" << std::endl;
//                output << writeHeadName << "++;" << std::endl;
//                output << "}" << std::endl; // end is string
//
//
//                // if a string copy for length?
//
//                output << "}" << std::endl; // end for
//
//                output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = " << output_buffer_name << ";" << std::endl; // TODO: no idea if it accepts .get<char>
//                output << "}" << std::endl; // end is_array
//            }
//
//
//
//            // char array stuff
//        }
//    }
//
//
//    // if type is scalar => output directly
//    if(type->isIntegerTy(1) || type->isIntegerTy(8) || type->isIntegerTy(16) || type->isIntegerTy(32) || type->isIntegerTy(64)){
//        output << "\t\t";
//        std::string parserArg = joinStrings(prefixes, GENERATE_FORMAT_CPP_ADDRESSING);
//
//        if(!isForGlobals) // only declare types if its not global, otherwise we introduce them as local variable
//            output << getCTypeNameForLLVMType(type) << " ";
//        else {
//
//            parserArg = "global." + parserArg;
//        }
//        //declare lvalue
//        output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE);
//        output << " = j" << joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING) << ".get<" << getCTypeNameForLLVMType(type) << ">();";
//        output << std::endl;
//    }
//
//    // if type is struct, recurse with member names added as prefix
//    if(type->isStructTy()){
//        // first do non array members
//        // declare and assign values to struct
//        // then fill in struct arrays (as these can only be filled in post declaration)
//
//
//        handsanitizer::DefinedStruct ds = getStructByLLVMType(type);
//        for(auto member : ds.getNamedMembers()){
//            if(member.second->isArrayTy() == false){
//                std::vector<std::string> fullMemberName(prefixes);
//                fullMemberName.push_back(member.first);
//                output << getParserRetrievalForNamedType(fullMemberName, member.second, false);
//            }
//        }
//        output << "\t";
//        if(!isForGlobals)
//            output << ds.definedCStructName << " ";
//        output  << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE);
//        if(isForGlobals)
//            output << " = { " << std::endl; // with globals we have different syntax :?
//        else
//            output << "{ " << std::endl;
//        for(auto member : ds.getNamedMembers()){
//            if(member.second->isArrayTy() == false){
//                std::vector<std::string> fullMemberName(prefixes);
//                fullMemberName.push_back(member.first);
//                //TODO formalize the fullmember name (lvalue name for member)
//                output << "\t\t" << "." << member.first << " = " << joinStrings(fullMemberName, GENERATE_FORMAT_CPP_VARIABLE) << "," << std::endl;
//            }
//        }
//        output << "\t};" << std::endl;
//
//        for(auto member : ds.getNamedMembers()){
//            // arrays can be also of custom types with arbitrary but finite amounts of nesting, so we need to recurse
//            if(member.second->isArrayTy()){
//                int arrSize = member.second->getArrayNumElements();
//                Type* arrType = member.second->getArrayElementType();
//                std::string iterator_name = getUniqueLoopIteratorName(); // we require a name from this function to get proper generation
//                output << "for(int " << iterator_name << " = 0; " << iterator_name << " < "<< arrSize << ";" << iterator_name << "++) {" << std::endl;
//                    std::vector<std::string> fullMemberName(prefixes);
//                    fullMemberName.push_back(member.first);
//                    fullMemberName.push_back(iterator_name);
//                    output << getParserRetrievalForNamedType(fullMemberName, arrType, false);
//                    output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << "." << member.first << "[" <<  iterator_name << "] = " << joinStrings(fullMemberName, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;
//                output << "}" << std::endl;
//            }
//        }
//    }
//    return output.str();
//}
//
//std::string getJsonObjectForScalar(handsanitizer::Type* type){
//    //TODO fix this for arrays
//    auto typeString = std::string("PLACEHOLDER");
//    for (auto & c: typeString) //didn't want to include boost::to_upper
//        c = toupper(c);
//    return "\"" + typeString + "_TOKEN" +  "\"" ;
//}
//
//std::string getJsonObjectForStruct(handsanitizer::DefinedStruct ds, int depth = 2){
//    std::stringstream s;
//    s << "{" << std::endl;
//    for(auto& mem : ds.getNamedMembers()){
//        for(int i = 0; i < depth; i++)
//            s << "\t";
//        s << "\"" << mem.first << "\": ";
//        if (mem.second->isStructTy()){
//            s << getJsonObjectForStruct(mem.second, depth+1);
//        }
//        else if(mem.second->isArrayTy()){
//            s << "[";
//            auto arrMemType = mem.second->getArrayElementType();
//            for(int i = 0; i < mem.second->getArrayNumElements(); i++){
//                if(arrMemType->isStructTy()){
//                    s << getJsonObjectForStruct(arrMemType);
//                }
//                else{
//                    s << getJsonObjectForScalar(arrMemType);
//                }
//                if (i != mem.second->getArrayNumElements() -1){ // names should be unique inside a struct
//                    s << ",";
//                }
//            }
//            s << "]";
//        }
//        else{
//            s << getJsonObjectForScalar(mem.second);
//        }
//        if (mem.first != ds.getNamedMembers().back().first){ // names should be unique inside a struct
//            s << ",";
//        }
//        s  << std::endl;
//    }
//    for(int i = 0; i < depth-1; i++)
//        s << "\t";
//    s << "}";
//    return s.str();
//}

//std::string getJsonInputTemplateText(std::vector<handsanitizer::Argument> args, std::vector<handsanitizer::GlobalVariable> globals){
//    std::stringstream s;
//    s << "{" << std::endl;
//
//    //TODO Fix this function
////    std::vector<handarg> all_elements = globals;
////    all_elements.insert(all_elements.end(), args.begin(), args.end());
////
////    for(auto& arg: all_elements){
////       s << "\t" << "\"" << arg.getName() << "\": ";
////        if (arg.getType()->isStructTy()){
////            s << getJsonObjectForStruct(getStructByLLVMType((StructType*)arg.getType()));
////        }
////        else if (arg.getType()->isPointerTy() && arg.getType()->getPointerElementType()->isStructTy()){
////            s << getJsonObjectForStruct(getStructByLLVMType((StructType*)arg.getType()->getPointerElementType()));
////        }
////        else if(arg.getType()->isArrayTy()){
////            s << "ARRAY_HERE";
////        }
////        else{
////            s << getJsonObjectForScalar(arg.getType());
////        }
////        // check if we are at the end of the iterator and skip trailing ','
////        // please let me know if you know something more elegant
////        if (&arg != &*all_elements.rbegin()){
////            s << "," << std::endl;
////        }
////
////    }
//
//    s << std::endl << "}";
//    return s.str();
//}

int main(int argc, char** argv){
    // parse llvm and open module
    llvm::InitLLVM X(argc, argv);
    ExitOnErr.setBanner(std::string(argv[0]) + ": error: ");
    llvm::LLVMContext Context;
    llvm::cl::ParseCommandLineOptions(argc, argv, "llvm .bc -> .ll disassembler\n");
    std::unique_ptr<llvm::MemoryBuffer> MB = ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFileOrSTDIN(InputFilename)));
    llvm::BitcodeFileContents IF = ExitOnErr(llvm::getBitcodeFileContents(*MB));
    std::cout << "Bitcode file contains this many modules " << IF.Mods.size() << std::endl;

    handsanitizer::ModuleFromLLVMModuleFactory factory;
    for(auto& llvm_mod : IF.Mods){
        std::unique_ptr<llvm::Module> mod = ExitOnErr(llvm_mod.parseModule(Context));
        auto extractedMod = factory.ExtractModule(Context, mod);


        std::cout << "Applying to mod " << std::endl;
        for(auto& f : extractedMod.functions){
            // TODO: use proper filesystem apis for filenames
            if(!std::filesystem::exists(OUTPUT_DIR))
                std::filesystem::create_directory(OUTPUT_DIR);

            std::cout << "generating for f" << std::endl;
            extractedMod.generate_cpp_file_for_function(f, OUTPUT_DIR + "/" + f.name + ".cpp");
            extractedMod.generate_json_input_template_file(f, OUTPUT_DIR + "/" + f.name + ".json");
        }
    }


//
//    int generatedFunctionCounter = 0;
//    for(auto& f : mod->functions()) {
//        std::string funcName = f.getName().str();
//        if(funcName.rfind("_Z",0) != std::string::npos){
//            std::cout << "function mangled " << funcName << std::endl;
//            continue; // mangeld c++ function
//        }
//        std::cout << "Found function: " << funcName << " pure: " << f.doesNotReadMemory() <<  " accepting guments: " << std::endl;
//
//
//        std::vector<handsanitizer::GlobalVariable> globals_of_func = extractGlobalValuesFromModule(mod);
//        std::vector<handsanitizer::Argument> args_of_func = extractArgumentsFromFunction(f);
//
//        // the return type might be a struct be an anonymous struct that is not defined in the bitcode module
//        llvm::Type* funcRetType = f.getReturnType();
//        if(funcRetType->isStructTy())
//            defineIfNeeded(funcRetType, true);
//
//        GenerateCppFunctionHarness(funcName, funcRetType, args_of_func, globals_of_func);
//        GenerateJsonInputTemplateFile(funcName, args_of_func, globals_of_func);
//        generatedFunctionCounter++;
//    }
//
//    return generatedFunctionCounter;
}

//void GenerateCppFunctionHarness(std::string &funcName, Type *funcRetType, std::vector<handarg> &args_of_func,
//                                std::vector<handarg> &globals_of_func) {
//    std::ofstream ofs;
//
//    ofs.open(output_file, std::ofstream::out | std::ofstream::trunc);
//    std::string setupFileString = getSetupFileText(funcName, funcRetType, args_of_func, globals_of_func);
//    ofs << setupFileString;
//    ofs.close();
//}

//std::vector<handsanitizer::GlobalVariable> extractGlobalValuesFromModule(std::unique_ptr<llvm::Module> &mod) {
//    std::vector<handsanitizer::GlobalVariable> globals;
//    for(auto& global : mod->global_values())
//    {
//        // TODO: Should figure out what checks actually should be in here
//        // Check if global is global value or something else,
//        // I remove function declarations here, but what if a global value is a function pointer?
//        if(!global.getType()->isFunctionTy()  && !(global.getType()->isPointerTy() && global.getType()->getPointerElementType()->isFunctionTy())  && global.isDSOLocal()){
//            globals.push_back(handsanitizer::GlobalVariable(global.getName(), 0 , global.getType()->getPointerElementType(), false));
//        }
//    }
//    return globals;
//}

//void GenerateJsonInputTemplateFile(const std::string &funcName, std::vector<handsanitizer::Argument> &args_of_func,
//                                   std::vector<handsanitizer::GlobalVariable> &globals) {
//    std::ofstream ofsj;
//    auto output_file_json = OUTPUT_DIR + "/" + funcName + ".json";
//    if(!std::filesystem::exists(OUTPUT_DIR))
//        std::filesystem::create_directory(OUTPUT_DIR);
//
//    ofsj.open(output_file_json, std::ofstream::out | std::ofstream::trunc);
//    ofsj << getJsonInputTemplateText(args_of_func, globals);
//    ofsj.close();
//}
//
//std::vector<handsanitizer::Argument> extractArgumentsFromFunction(llvm::Function &f) {
//    std::vector<handsanitizer::Argument> args;
//    int argcounter = 0;
//    for(auto& arg : f.args()){
//        std::string argName = "";
//        if(arg.hasName())
//            argName = arg.getName();
//        else
//            argName = "e_" + std::to_string(argcounter);
//
//        // TODO: Make sure that these names don't conflict with globals
////        std::cout << "Name: " << argName << " type: " << printRawLLVMType(arg.getType()) << std::endl;
//        args.push_back(handsanitizer::Argument(argName, arg.getArgNo(), arg.getType(), arg.hasByValAttr()));
//        argcounter++;
//    }
//    return args;
//}





/*
 * The format of the generated file is:
 * header includes
 * struct declarations
 * test_function declaration
 * helper functions used in generated text
 * global variable setup (including the parser)
 * setupParser Function
 * callFunction Function
 */
//std::string getSetupFileText(std::string functionName, handsanitizer::Type *funcRetType, std::vector<handsanitizer::Argument> &args_of_func,
//                             std::vector<handsanitizer::GlobalVariable> &globals) {
//    std::stringstream setupfilestream;
//
//    // INCLUDES HEADERS
//    setupfilestream << "#include \"skelmain.hpp\" " << std::endl;
//    setupfilestream << "#include <nlohmann/json.hpp> // header only lib" << std::endl << std::endl;
//
//
//    // DECLARE STRUCTS
//
//    defineStructs(globals);
//    defineStructs(args_of_func);
//
//    setupfilestream << getStructDefinitions() << std::endl;
//
//    // DECLARATION TEST FUNCTION
//    // the LLVM type name differs from how we define it in our generated source code
//    std::string rettype = getCTypeNameForLLVMType(funcRetType);
//    if(funcRetType->isStructTy())
//        rettype = getStructByLLVMType((StructType*)funcRetType).definedCStructName;
//    std::string functionSignature = rettype + " " +  functionName + "(" + getTypedArgumentNames(args_of_func) + ");";
//    setupfilestream << "extern \"C\" " + functionSignature << std::endl;
//
//    // DECLARE GLOBALS
//    setupfilestream << "argparse::ArgumentParser parser;" << std::endl << std::endl;
//    setupfilestream << "// globals from module" << std::endl << getDefineGlobalsText(globals)  << std::endl << std::endl;
//
//
//    // DEFINE SETUPPARSER
//    setupfilestream << "void setupParser() { " << std::endl
//            << "std::stringstream s;"
//            << "s << \"Test program for: " << functionSignature << "\" << std::endl << R\"\"\"("  << getStructDefinitions() << ")\"\"\";" << std::endl
//            << "\tparser = argparse::ArgumentParser(s.str());" << std::endl
//            << "} " << std::endl << std::endl;
//
//    // DEFINE CALLFUNCTION
//    setupfilestream << "void callFunction() { " << std::endl
//    << "\t\t" << "std::string inputfile = parser.get<std::string>(\"-i\");" << std::endl
//    << "\t\t" << "std::ifstream i(inputfile);\n"
//    "\t\tnlohmann::json j;\n"
//    "\t\ti >> j;\n"
//    << getParserRetrievalText(globals, true)
//    << getParserRetrievalText(args_of_func, false);
//
//    auto output_var_name = "output";
//    setupfilestream << "auto " << output_var_name << " = " <<  functionName << "(" << getUntypedArgumentNames(args_of_func) << ");" << std::endl;
//    setupfilestream <<  getJsonOutputText(output_var_name, funcRetType);
//
//    setupfilestream << "\t\t" << "std::cout << output_json << std::endl;" << std::endl;
//
//
//    setupfilestream << "} " << std::endl;
//    std::string setupFileString = setupfilestream.str();
//    return setupFileString;
//}
