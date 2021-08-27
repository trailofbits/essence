cmake_minimum_required(VERSION 3.19)
project(handsan)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM 11 REQUIRED)
add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})



add_executable(handsan handsanitizer/LLVMExtractor.hpp handsanitizer/LLVMExtractor.cpp handsanitizer/FunctionCallerGenerator.cpp handsanitizer/FunctionCallerGenerator.h handsanitizer/JsonInputParser.cpp handsanitizer/JsonInputParser.h handsanitizer/JsonOutputGenerator.cpp handsanitizer/JsonOutputGenerator.h handsanitizer/DeclarationManager.h handsanitizer/DeclarationManager.cpp handsanitizer/SpecificationPrinter.cpp handsanitizer/SpecificationPrinter.h)
target_include_directories(handsan PRIVATE vendor/include)

target_sources(handsan PRIVATE handsanitizer/main.cpp)


target_link_libraries(handsan PRIVATE
        LLVM)
