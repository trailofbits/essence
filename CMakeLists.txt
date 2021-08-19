cmake_minimum_required(VERSION 3.19)
project(HandSanitizer)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY .)

find_package(LLVM 11 REQUIRED)
add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

find_package(nlohmann_json CONFIG REQUIRED)
include_directories(argparse/include)



add_executable(HandSanitizer handsanitizer/LLVMExtractor.hpp handsanitizer/LLVMExtractor.cpp handsanitizer/FunctionCallerGenerator.cpp handsanitizer/FunctionCallerGenerator.h handsanitizer/JsonInputParser.cpp handsanitizer/JsonInputParser.h handsanitizer/JsonOutputGenerator.cpp handsanitizer/JsonOutputGenerator.h handsanitizer/DeclarationManager.h handsanitizer/DeclarationManager.cpp handsanitizer/SpecificationPrinter.cpp handsanitizer/SpecificationPrinter.h)
target_sources(HandSanitizer PRIVATE
        handsanitizer/main.cpp)


target_link_libraries(HandSanitizer PRIVATE
        LLVM
        nlohmann_json nlohmann_json::nlohmann_json)
