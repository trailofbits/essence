//
// Created by sabastiaan on 24-06-21.
//

#ifndef skelmain_cpp_
#define skelmain_cpp_
#include "skelmain.hpp"
#include <iostream>


int main(int argc, char *argv[]) {
    setupParser();

    try {
        parser.parse_args(argc, argv);
    }
    catch (const std::runtime_error& err) {
        std::cout << err.what() << std::endl;
        std::cout << parser;
        exit(0);
    }

    callFunction();
    return 0;
}
#endif