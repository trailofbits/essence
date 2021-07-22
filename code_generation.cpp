//
// Created by sabastiaan on 19-07-21.
//

#include "include/code_generation.hpp"
using namespace llvm;
std::string OUTPUT_DIR = "output";

std::string getStringFromJson(std::string output_var, std::string json_name, std::string tmp_name_1, std::string tmp_name_2, bool addNullTermination){
    std::stringstream s;
    s << "std::string " << tmp_name_1 << " = " << json_name << ".get<std::string>();" << std::endl;
    s << "std::vector<char> " << tmp_name_2 << "(" << tmp_name_1 << ".begin(), " << tmp_name_1 << ".end());";
    if(addNullTermination)
        s << tmp_name_2 << ".push_back('\\x00');" << std::endl;
    s << "char* " << output_var << " = &" << tmp_name_2 << "[0];" << std::endl;
    return s.str();
}


std::string getStringLengthFromJson(std::string output_var, std::string json_name, std::string tmp_name_1, std::string tmp_name_2, bool addNullTermination){
    std::stringstream s;
    s << "std::string " << tmp_name_1 << " = " << json_name << ".get<std::string>();" << std::endl;
    s << "std::vector<char> " << tmp_name_2 << "(" << tmp_name_1 << ".begin(), " << tmp_name_1 << ".end());";
    if(addNullTermination)
        s << tmp_name_2 << ".push_back('\x00');" << std::endl;
    s << "int " << output_var << " = " << tmp_name_2 << ".size();" << std::endl;
    return s.str();
}
