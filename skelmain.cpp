//
// Created by sabastiaan on 24-06-21.
//

#ifndef skelmain_cpp_
#define skelmain_cpp_
#include "skelmain.hpp"
#include <iostream>


int main(int argc, char *argv[]) {
    bool isJsonInput = false;
    if(argc > 1){
        auto firstArg = std::string(argv[1]);
        //check it manually like this because argparse doesn't support toggleable required inputs
        if(firstArg.compare("-i") == 0 || firstArg.compare("--input"))
            isJsonInput = true;
    }

    setupParser(isJsonInput);
    try {
        parser.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cout << err.what() << std::endl;
        std::cout << parser;
        exit(0);
    }
    callFunction(isJsonInput);
    return 0;
}
#endif