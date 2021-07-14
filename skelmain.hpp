//
// Created by sabastiaan on 24-06-21.
//

#ifndef HANDSANITIZER_SUM_H
#define HANDSANITIZER_SUM_H

#include <argparse/argparse.hpp>
#include <fstream>
extern argparse::ArgumentParser parser;

void setupParser(bool isJsonInput);
void callFunction(bool isJsonInput);
#endif //HANDSANITIZER_SUM_H
