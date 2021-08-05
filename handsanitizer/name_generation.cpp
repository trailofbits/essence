//
// Created by sabastiaan on 19-07-21.
//

#include "../include/name_generation.hpp"




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

    bool hasSkippedRoot = false;

    for(std::string s : strings){
        if(format != GENERATE_FORMAT_CPP_VARIABLE && s == POINTER_DENOTATION)
            continue; // don't print the pointer markings in lvalue as these are abstracted away
        if(format == GENERATE_FORMAT_JSON_ARRAY_ADDRESSING || format == GENERATE_FORMAT_JSON_ARRAY_ADDRESSING_WITHOUT_ROOT){
            if(format == GENERATE_FORMAT_JSON_ARRAY_ADDRESSING_WITHOUT_ROOT && !hasSkippedRoot){
                hasSkippedRoot = true;
                continue;
            }
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
    if(retstring.length() > 0 && format != GENERATE_FORMAT_JSON_ARRAY_ADDRESSING && format != GENERATE_FORMAT_JSON_ARRAY_ADDRESSING_WITHOUT_ROOT)
        retstring.pop_back(); //remove trailing delimiter
    return retstring;
}