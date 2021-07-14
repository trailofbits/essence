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

using namespace llvm;
static ExitOnError ExitOnErr;

static cl::opt<std::string>
        InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));


std::string getCTypeNameForLLVMType(Type* type){
    if(type->isPointerTy())
        return getCTypeNameForLLVMType(type->getPointerElementType()) + "*";

//    we don't get a parser for bool
//    if(type->isIntegerTy(1))
//        return "bool";

    if(type->isIntegerTy(8))
        return "char";

    if(type->isIntegerTy(16))
        return "short";

    if(type->isIntegerTy(32))
        return "int";

    if(type->isIntegerTy(64))
        return "long";

    if(type->isIntegerTy(64))
        return "long long"; // this makes very strong assumptions on underling structures, should fix this

    if(type->isVoidTy())
        return "void";

    if(type->isFloatTy())
        return "float";

    if(type->isFloatTy())
        return "double";

    if(type->isStructTy()){
        std::string s = type->getStructName();
        if(s.find("struct.") != std::string::npos)
            s = s.replace(0, 7, ""); //removes struct. prefix

        else if (s.find("union.") != std::string::npos)
            s = s.replace(0, 6, ""); //removes union. prefix
        return s;
    }

    return "Not supported";
}


std::string parseCharHelperFunctionText = R"""(
char parseCharInput(const std::string& value){
    if (value.size() > 2 && value[0] == '\'' && value[2] == '\'') {
        return value.at(1);
    }

    if(value.compare("true") == 0){
        return (char)1;
    }
    if(value.compare("false") == 0){
        return (char)0;
    }

    int i = std::stoi(value);
    if (i <= 127 & i >= -128) // there is no std::stoc :/
        return (char)i;

    throw std::invalid_argument("value does not fit in char");
}
)""";

std::string parseShortHelperFunctionText = R"""(
short parseShortInput(const std::string& value){
    int i = std::stoi(value);
    if (i <= 32767 & i >= -32768) // there is no std::stoc :/
        return (short)i;

    throw std::invalid_argument("value does not fit in char");
}
)""";
//
//namespace handsanitizer{
//    class argument{
//    public:
//        std::string unique_name;
//        argument(std::string name){
//            this->unique_name = unique_name;
//        }
//
//        // this only works for types
//        virtual std::string getCTypeName() = 0;
//        // also types + names
//        virtual std::string getParserSetupText() = 0;
//        // types + variable names
//        virtual std::string getParserValueText() = 0;
//    };
//
//    class CharType : argument{
//         std::string getCTypeName() override{
//             return "char";
//         };
//         std::
//
//    };
//}
//



// llvm arguments are context bound so we can't use them as freely
// therefore we keep all info in a custom structure
// should it include a list for structs S.T each struct has members+name, and can I use that setup for scalar values as well?
class handarg {
public:
    std::string name;
    int position;
    Type* type;
    bool passByVal;

    handarg(std::string name, int position, Type* type, bool passByVal = false){
        this->name = name;
        this->position = position;
        this->type = type;
        this->passByVal = passByVal;
    };

    //mirror some LLVM type functions
    Type* getType(){ return this->type;};
    Type* getTypeOrPointedType(){
        if(this->type->isPointerTy())
            return this->type->getPointerElementType();
        return this->type;
    };
    bool isStructOrStructPtr(){ return this->type->isStructTy() || (this->type->isPointerTy() && this->type->getPointerElementType()->isStructTy());}
    std::string getName(){ return this->name;};
};



// if we encounter a struct or union type we must define it first in our program
// we do this through a DFS on the elements of every argument we find
// as long as the result of every DFS search on another root/non-recursive call is appended AFTER the previous result
// we won't get into a nasty dependency problem. This follows trivially from that at each point
// we either generate direct dependencies or they are already defined before hand.
// To not re-define previously defined structs we save the name in an vector



// Types in llvm are unique, therefore we can create an bijective DefinedStruct -> StructType
// I hope this uniqueness is also for structs
// and save all the member names in DefinedStruct
class DefinedStruct {
public:
    bool isUnion = false;
    StructType* structType;
    std::string definedCStructName;
    std::vector<std::string> member_names; // implicitly ordered, can I make an tuple out of this combined with the member?

    std::vector<std::pair<std::string, Type*>> getNamedMembers() {
        std::vector<std::pair<std::string, Type*>> output;
        if(member_names.size() != structType->getNumElements())
            std::throw_with_nested("Not all members are named");

        for(int i = 0; i < structType->getNumElements(); i++){
            output.push_back(std::pair<std::string, Type*>(member_names[i], structType->getStructElementType(i)));

        }
        return output;
    };
};

enum StringFormat{
    GENERATE_FORMAT_CLI,
    GENERATE_FORMAT_CPP_VARIABLE,
    GENERATE_FORMAT_JSON_ARRAY_ADDRESSING
};

static std::string CLI_NAME_DELIMITER = ".";
static std::string LVALUE_DELIMITER = "_";
static std::string POINTER_DENOTATION = "__p";
// so big idea is to generate all source pieces from list<args>
// with args = struct{ LLVMType, position, name}, and at each c++ location we need one to call ConvertLLVMTypeToC++,
std::string joinStrings(std::vector<std::string> strings, StringFormat format){
    std::stringstream output;
    std::string delimiter = "";
    if(format == GENERATE_FORMAT_CLI)
        delimiter = CLI_NAME_DELIMITER;
    if(format == GENERATE_FORMAT_CPP_VARIABLE)
        delimiter = LVALUE_DELIMITER;


    for(std::string s : strings){
        if(format != GENERATE_FORMAT_CPP_VARIABLE && s == POINTER_DENOTATION)
            continue; // don't print the pointer markings in lvalue as these are abstracted away
        if(format == GENERATE_FORMAT_JSON_ARRAY_ADDRESSING){
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

// Unions get translated to structs in LLVM so this should also cover that
std::vector<DefinedStruct> definedStructs;


bool isStructAlreadyDefined(StructType* type){
    for(auto s : definedStructs){
        if(s.structType == type)
            return true;
    }
    return false;
}



DefinedStruct getStructByLLVMType(StructType* structType){
    for(auto s : definedStructs){
        if(s.structType == structType)
            return s; // will this deep copy the vector as well?
    }
    std::throw_with_nested("Struct is not defined");
}
std::stringstream definitionStrings;

// Adds itself in the correct order to definitionStream
void defineIfNeeded(Type* arg, bool isRetType = false){
    //extract if its pointer type
    if(arg->isPointerTy())
        arg = arg->getPointerElementType();

    std::cout << "called for: " << getCTypeNameForLLVMType(arg);
    if(arg->isStructTy()){ // we don't care about arrays thus no (is aggregrate type)
        std::cout << " Found a struct type";

        if(isStructAlreadyDefined((StructType*) arg))
            return;
        std::cout << "Not defined";
        if(arg->getStructNumElements() > 0){
            std::cout << "Has members";
            std::stringstream  elementsString;
            DefinedStruct newDefinedStruct;
            if(arg->getStructName().find("union.") != std::string::npos)
                newDefinedStruct.isUnion = true;

            // all members need to be added, if any struct is referenced we need tto define it again
            for(int i =0 ; i < arg->getStructNumElements(); i++){
                auto child = arg->getStructElementType(i);
                // is struct or struct*
                if (child->isStructTy() || (child->isPointerTy() && child->getPointerElementType()->isStructTy()))
                    defineIfNeeded(child);

                std::string childname = "e_" + std::to_string(i);
                elementsString << "\t" << getCTypeNameForLLVMType(child) << " " << childname  << ";" << std::endl;
                newDefinedStruct.member_names.push_back(childname);
            }

            // TODO: use getCPPName instead of getStructName?
            auto structName = getCTypeNameForLLVMType(arg);
            if(isRetType)
                structName = "returnType";
            newDefinedStruct.structType = (StructType*) arg;
            if(newDefinedStruct.isUnion)
                definitionStrings << "typedef union ";
            else
                definitionStrings << "typedef struct ";
            definitionStrings << structName << " { " << std::endl << elementsString.str() << "} " << structName << ";" <<  std::endl <<  std::endl;
            newDefinedStruct.definedCStructName = structName;
            definedStructs.push_back(newDefinedStruct);
        }
    }
}

std::string getJSONStructDeclartions(){
    std::stringstream s;

    for(auto& ds : definedStructs){
        s << "NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(";
        s << ds.definedCStructName << ", ";
        for(auto& mem : ds.getNamedMembers()){
            s << mem.first << ", ";
        }
        s.seekp(-2,s.cur); // removes trialing , by replacing it with )
        s << ")" << std::endl;
    }
    return s.str();
}

std::string getStructDefinitions(std::vector<handarg> args){
    for(auto& a : args){
        defineIfNeeded(a.getType());
    }

    return definitionStrings.str();
}

std::vector<handarg> globals;
std::string getDefineGlobalsText(){
    std::stringstream s;
    for(auto& global : globals){
        s << "extern " << getCTypeNameForLLVMType(global.getType()) << " " << global.getName() << ";" << std::endl;
    }

    return s.str();
}


// thus we only need a function which converts has a one-to-one with LLVM to c++ types,

std::string getParserSetupTextForScalarType(std::string name, Type* type, bool isGlobal = false){
    std::stringstream s;
    std::string requiredString = ".required()";
    if(isGlobal){
        name = "global." + name;
        requiredString = ""; //dirty hack
    }
    std::string action = ""; //parsing method

    if(type->isIntegerTy(8))
        action = "parseCharInput";

    if(type->isIntegerTy(16))
        action = "parseShortInput";

    if(type->isIntegerTy(32))
        action = "[](const std::string& value) {return stoi(value);}";

    if(type->isIntegerTy(64))
        action = "[](const std::string& value) {return stol(value);}";

    if(type->isFloatTy())
        action = "[](const std::string& value) {return stof(value);}";

    if(type->isDoubleTy())
        action = "[](const std::string& value) {return stod(value);}";


    if(type->isIntegerTy(8) || type->isIntegerTy(16) || type->isIntegerTy(32) || type->isIntegerTy(64) || type->isFloatTy() || type->isDoubleTy()){
        s << "\t\t" << "parser.add_argument(\"--" << name << "\")" << requiredString <<
                                                           ".help(\"" << getCTypeNameForLLVMType(type) << "\")" <<
                                                           ".action(" << action << ");" << std::endl;
    }

    return s.str();
}




std::string getParserSetupTextFromLLVMTypes(std::vector<std::string> name_prefix, Type* arg, bool isGlobal = false){
    std::stringstream s;

    if(arg->isStructTy()){
        auto str = getStructByLLVMType((StructType*) arg);
        for(auto& mem : str.getNamedMembers()){
            std::vector<std::string> memberName(name_prefix);
            memberName.push_back(mem.first);
            s << getParserSetupTextFromLLVMTypes(memberName, mem.second, isGlobal);
        }
    }

    if(arg->isPointerTy() && arg->getPointerElementType()->isStructTy()){
//        auto structType = arg;
        auto str = getStructByLLVMType((StructType*) arg->getPointerElementType());
        for(auto& mem : str.getNamedMembers()){
            std::vector<std::string> memberName(name_prefix);
            memberName.push_back(mem.first);
            s << getParserSetupTextFromLLVMTypes(memberName, mem.second, isGlobal); // but we need to maintain the counter
        }
//            std::string argname = name_prefix + "p" + std::to_string(counter);
        // we dont encode pointers in name so just call recursively

    }
    else{
        if(arg->isPointerTy())
            s << getParserSetupTextFromLLVMTypes(name_prefix, arg->getPointerElementType(), isGlobal); // but we need to maintain the counter
        else
            s << getParserSetupTextForScalarType(joinStrings(name_prefix, GENERATE_FORMAT_CLI), arg, isGlobal);
    }


    return s.str();
}

// should include position in loop instead of relying on implicit order
// wrapper around getParserSetupTextFromType
std::string getParserSetupTextFromHandargs(std::vector<handarg> args, bool isForGlobals = false){
    std::stringstream s;
    for(auto& a : args){
        std::vector<std::string> name;
        name.push_back(a.getName());
        s << getParserSetupTextFromLLVMTypes(name,a.type, isForGlobals);
    }
    return s.str();
}


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
std::string getParserRetrievalForNamedType(std::vector<std::string> prefixes, Type* type, bool isForGlobals = false, bool jsonInput = false){
    std::stringstream output;


    // if is pointer, allocate the value and address it directly from the stack
    // expand this such that it allows for arrays
    if(type->isPointerTy()){
        std::vector<std::string> referenced_name(prefixes);
        referenced_name.push_back(POINTER_DENOTATION);
        output << getParserRetrievalForNamedType(referenced_name, type->getPointerElementType(), isForGlobals, jsonInput);

        output << "\t\t";
        if(!isForGlobals)
            output << getCTypeNameForLLVMType(type) << " ";
        output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << " = &" << joinStrings(referenced_name, GENERATE_FORMAT_CPP_VARIABLE) << ";" << std::endl;
    }


    // if type is scalar => output directly
    if(type->isIntegerTy(1) || type->isIntegerTy(8) || type->isIntegerTy(16) || type->isIntegerTy(32) || type->isIntegerTy(64)){
        output << "\t\t";
        std::string parserArg = joinStrings(prefixes, GENERATE_FORMAT_CLI);

        if(!isForGlobals) // only declare types if its not global, otherwise we introduce them as local variable
            output << getCTypeNameForLLVMType(type) << " ";
        else {

            parserArg = "global." + parserArg;
            if(!jsonInput) //wrap in parser.is_used to prevent exceptions on unspecified defined globals, only relevant for the parser, we could get rid of this by using defaults?
                output << "if(parser.is_used(\"--" << parserArg << "\")){ " << std::endl << "\t\t";
        }
        //declare lvalue
        output << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE);
        if(jsonInput)
            output << " = j" << joinStrings(prefixes, GENERATE_FORMAT_JSON_ARRAY_ADDRESSING) << ".get<" << getCTypeNameForLLVMType(type) << ">();";
        else
            output << " = parser.get<" << getCTypeNameForLLVMType(type) << ">(\"--" <<  parserArg << "\");";
        output << std::endl;
        if(isForGlobals && !jsonInput)
            output << "\t\t}" << std::endl;


    }


    // if type is struct, recurse with member names added as prefix
    if(type->isStructTy()){
        DefinedStruct ds = getStructByLLVMType((StructType*)type);
        for(auto member : ds.getNamedMembers()){
            std::vector<std::string> fullMemberName(prefixes);
            fullMemberName.push_back(member.first);
            output << getParserRetrievalForNamedType(fullMemberName, member.second, isForGlobals, jsonInput);
        }
        output << "\t";
        if(!isForGlobals)
            output << ds.definedCStructName << " ";
        output  << joinStrings(prefixes, GENERATE_FORMAT_CPP_VARIABLE) << "{ " << std::endl;
        for(auto member : ds.getNamedMembers()){
            std::vector<std::string> fullMemberName(prefixes);
            fullMemberName.push_back(member.first);
            //TODO formalize the fullmember name (lvalue name for member)
            output << "\t\t" << "." << member.first << " = " << joinStrings(fullMemberName, GENERATE_FORMAT_CPP_VARIABLE) << "," << std::endl;
        }
        output << "\t};" << std::endl;

    }
    return output.str();
}

std::string getParserRetrievalText(std::vector<handarg> args, bool isForGlobals = false, bool jsonInput = false){
    std::stringstream s;

    for(auto& a : args){
        std::vector<std::string> dummy;
        dummy.push_back(a.getName());

        // llvm boxes structs always with a pointer?
        if(a.type->isPointerTy() && a.type->getPointerElementType()->isStructTy())
            s << getParserRetrievalForNamedType(dummy, a.type->getPointerElementType(), isForGlobals, jsonInput);
        else{
            s << getParserRetrievalForNamedType(dummy, a.type, isForGlobals, jsonInput);
        }
    }
    return s.str();
        // first declare all sub structures
        // wrap pointer values
        // deal with passbyref/passbyval
//        if(a.getType()->isPointerTy() && a.getType()->getPointerElementType()->isStructTy()){
            //ignore their pointers for now

//            auto structTypePtr = (StructType*)a.getType()->getPointerElementType(); // if its a pointer type this fails
//            std::stringstream subelementstream;
//            // I need to know the names beforehand?
//            auto ds = getStructByLLVMType(structTypePtr);



            //this will take

            /*
             * this will take struct X{ int a, char b}
             * int Xa = parser.get<int>("--X.a")
             * char Xb = parser.get<char>("--X.b")
             * Struct X = { .a = Xa; .b=Xb}
             *
             * how do we get the name s for a and b?
             * we might actually need to end up naming the entire tree
             * as for an func arg(X x), it just needs a valid X, but does not care about naming
             * even if we inline, the parser.get(name) requires us to specifiy exactly what is in a through the tree
             * although root + type tree would be enough?  -- yes, but ugly as this would  make the conversion for
             * between names an implicit rule which must be valid and is just very fragile in the beginning
             * probably better to explicitly name everything
             */
}


std::string getTypedArgumentNames(std::vector<handarg> args){
    std::stringstream s;
    for(auto& a : args){
        if(a.passByVal && a.getType()->isPointerTy())
            s << getCTypeNameForLLVMType(a.getType()->getPointerElementType()) << ' ' << a.getName() << ',';
        else
            s << getCTypeNameForLLVMType(a.getType()) << ' ' << a.getName() << ',';
    }
    auto retstring = s.str();
    if(!retstring.empty())
        retstring.pop_back(); //remove last ,
    return retstring;
}

std::string getUntypedArgumentNames(std::vector<handarg> args){
    std::stringstream s;
    for(auto& a : args){
        s << a.getName()<< ',';
    }
    auto retstring = s.str();
    if(!retstring.empty())
        retstring.pop_back(); //remove last ,
    return retstring;
}

std::string getJsonObjectForScalar(Type* type){
    auto typeString = std::string(getCTypeNameForLLVMType(type));
    for (auto & c: typeString) //didn't want to include boost::to_upper
        c = toupper(c);
    return "\"" + typeString + "_TOKEN" +  "\"" ;
}

std::string getJsonObjectForStruct(DefinedStruct ds, int depth = 2){
    std::stringstream s;
    s << "{" << std::endl;
    for(auto& mem : ds.getNamedMembers()){
        for(int i = 0; i < depth; i++)
            s << "\t";
        s << "\"" << mem.first << "\": ";
        if (mem.second->isStructTy()){
            s << getJsonObjectForStruct(getStructByLLVMType((StructType*)mem.second), depth+1);
        }
        else{
            s << getJsonObjectForScalar((StructType*)mem.second);
        }
        if (mem.first != ds.getNamedMembers().back().first){ // names should be unique inside a struct
            s << ",";
        }
        s  << std::endl;
    }
    for(int i = 0; i < depth-1; i++)
        s << "\t";
    s << "}";
    return s.str();
}

std::string getJsonInputTemplateText(std::vector<handarg> args, std::vector<handarg> globals){
    std::stringstream s;
    s << "{" << std::endl;
    for(auto& arg: args){
       s << "\t" << "\"" << arg.getName() << "\": ";
        if (arg.getType()->isPointerTy() && arg.getType()->getPointerElementType()->isStructTy()){
            s << getJsonObjectForStruct(getStructByLLVMType((StructType*)arg.getType()->getPointerElementType()));
        }
        else{
            s << getJsonObjectForScalar(arg.getType());
        }
        // check if we are at the end of the iterator and skip trailing ','
        // please let me know if you know something more elegant
        if (&arg != &*args.rbegin()){
            s << "," << std::endl;
        }

    }

    s << std::endl << "}";
    return s.str();
}



int main(int argc, char** argv){
    InitLLVM X(argc, argv);
    ExitOnErr.setBanner(std::string(argv[0]) + ": error: ");

    LLVMContext Context;

    cl::ParseCommandLineOptions(argc, argv, "llvm .bc -> .ll disassembler\n");


    std::unique_ptr<MemoryBuffer> MB =
            ExitOnErr(errorOrToExpected(MemoryBuffer::getFileOrSTDIN(InputFilename)));

    BitcodeFileContents IF = ExitOnErr(llvm::getBitcodeFileContents(*MB));

    std::cout << "Bitcode file contains this many modules " << IF.Mods.size() << std::endl;

    std::unique_ptr<Module> mod = ExitOnErr(IF.Mods[0].parseModule(Context));
    int count = 0;

    auto TargetTriple = sys::getDefaultTargetTriple();
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    std::string Error;
    auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);

// Print an error and exit if we couldn't find the requested target.
// This generally occurs if we've forgotten to initialise the
// TargetRegistry or we have a bogus target triple.
    if (!Target) {
        errs() << Error;
        return 1;
    }


    auto CPU = "generic";
    auto Features = "";

    TargetOptions opt;
    auto RM = Optional<Reloc::Model>(Reloc::DynamicNoPIC);

    //normal linux triple




    for(auto& f : mod->functions()) {
        if(f.getName().str().rfind("_Z",0) != std::string::npos){
            std::cout << "function mangled " << f.getName().str() << std::endl;
            continue; // mangeld c++ function
        }

        std::vector<handarg> args_of_func;
        std::ofstream ofs;
        auto output_file = std::string(f.getName());
        output_file = output_file + ".cpp";
        ofs.open(output_file, std::ofstream::out | std::ofstream::trunc);

        Type* funcRetType = f.getReturnType();
        if(funcRetType->isStructTy())
            defineIfNeeded(funcRetType, true);

        std::cout << "Found function: " << f.getName().str() << " pure: " << f.doesNotReadMemory() <<  " accepting arguments: " << std::endl;
        for(auto& global : mod->global_values())
        {
            if(!global.getType()->isFunctionTy()  && !(global.getType()->isPointerTy() && global.getType()->getPointerElementType()->isFunctionTy())  && global.isDSOLocal()){ // should probably do more checks
                globals.push_back(handarg(global.getName(), 0 , global.getType()->getPointerElementType(), false));
            }
        }


        int argcounter = 0;
        for(auto& arg : f.args()){

            std::string type_str;
            llvm::raw_string_ostream rso(type_str);
            arg.getType()->print(rso);
            std::string argname = "";
            if(arg.hasName())
                argname = arg.getName();
            else
                argname = "e_" + std::to_string(argcounter);

            // TODO: Make sure that these names don't conflict with globals
            std::cout << "Name: " << argname << " type: " << rso.str() << std::endl;
            args_of_func.push_back(handarg(argname, arg.getArgNo(), arg.getType(), arg.hasByValAttr()));
            argcounter++;
        }

        count++;


        if (f.getName().str() == "test"){
            std::cout << "can do smth with test";

            auto Callee = f.getParent()->getOrInsertFunction("callfunc", Type::getVoidTy(mod->getContext()));
            auto Fun = dyn_cast<Constant>(Callee.getCallee());
            IRBuilder<> builder(&*f.getEntryBlock().getFirstInsertionPt());
            builder.CreateCall(Fun, None);
        }


        //print main

        std::stringstream setupfilestream;
        setupfilestream << "#include \"skelmain.hpp\" " << std::endl;
        setupfilestream << "#include <nlohmann/json.hpp> // header only lib" << std::endl << std::endl;
        setupfilestream << getStructDefinitions(args_of_func) << std::endl;

        setupfilestream << "// still need to fix return types" << std::endl;

        std::string rettype = getCTypeNameForLLVMType(funcRetType);
        if(funcRetType->isStructTy())
            rettype = getStructByLLVMType((StructType*)funcRetType).definedCStructName;
            std::string functionSignature = rettype + " " +  f.getName().str() + "(" + getTypedArgumentNames(args_of_func) + ");";
        setupfilestream << "extern \"C\" " + functionSignature << std::endl;
        setupfilestream << "argparse::ArgumentParser parser;" << std::endl << std::endl;
        setupfilestream << "// globals from module" << std::endl << getDefineGlobalsText()  << std::endl << std::endl;
        setupfilestream << getJSONStructDeclartions() << std::endl;

        setupfilestream << parseCharHelperFunctionText << std::endl << std::endl;
        setupfilestream << parseShortHelperFunctionText << std::endl << std::endl;
        //extend parser stuff to include globals

        setupfilestream << "void setupParser(bool inputIsJson) { " << std::endl
                << "std::stringstream s;"
                << "s << \"Test program for: " << functionSignature << "\" << std::endl << R\"\"\"("  << getStructDefinitions(args_of_func) << ")\"\"\";" << std::endl
                << "\tparser = argparse::ArgumentParser(s.str());" << std::endl
                << "\t" << "if(inputIsJson)" << std::endl << "\t\t" << "parser.add_argument(\"-i\", \"--input\").required();" << std::endl
                << "\t" << "else { " << std::endl
                << getParserSetupTextFromHandargs(globals, true)
                << getParserSetupTextFromHandargs(args_of_func)
                << "\t}" << std::endl
                << "} " << std::endl << std::endl;
        setupfilestream << "void callFunction(bool inputIsJson) { " << std::endl
        << "\t" << "if(inputIsJson){" << std::endl
        << "\t\t" << "std::string inputfile = parser.get<std::string>(\"-i\");" << std::endl
        << "\t\t" << "std::ifstream i(inputfile);\n"
        "\t\tnlohmann::json j;\n"
        "\t\ti >> j;\n"
        << getParserRetrievalText(globals, true, true)
        << getParserRetrievalText(args_of_func, false, true)
        << "\t\t" << "nlohmann::json output_json = " << f.getName().str() << "(" << getUntypedArgumentNames(args_of_func) << ");" << std::endl
        << "\t\t" << "std::cout << output_json << std::endl;" << std::endl
        << "\t}" << std::endl
        << "\telse {" << std::endl
        << getParserRetrievalText(globals, true)
        << getParserRetrievalText(args_of_func)
        << "\t\t" << "nlohmann::json output_json = " << f.getName().str() << "(" << getUntypedArgumentNames(args_of_func) << ");" << std::endl
        << "\t\t" << "std::cout << output_json << std::endl;" << std::endl

        << "\t" << "}"
        << std::endl;

        setupfilestream << "} " << std::endl;



        ofs << setupfilestream.str();
        ofs.close();

        std::ofstream ofsj;
        auto output_file_json = std::string(f.getName());
        output_file_json = output_file_json + ".json";
        ofsj.open(output_file_json, std::ofstream::out | std::ofstream::trunc);
        ofsj << getJsonInputTemplateText(args_of_func, globals);
        ofsj.close();

    }


    auto TargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
    mod->setDataLayout(TargetMachine->createDataLayout());
    mod->setTargetTriple(TargetTriple);

    //define output
    auto Filename = "output.o";
    std::error_code EC;
    raw_fd_ostream dest(Filename, EC, sys::fs::OF_None);

    if (EC) {
        errs() << "Could not open file: " << EC.message();
        return 1;
    }

    legacy::PassManager pass;
    if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, CGFT_ObjectFile)) {
        errs() << "TargetMachine can't emit a file of this type";
        return 1;
    }

    pass.run(*mod);
    dest.flush();


    return count;
}
