cmake_minimum_required(VERSION 3.19)
project(HandSan)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM 10 REQUIRED)
add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

find_package(nlohmann_json CONFIG REQUIRED)



add_executable(HandSan handsanitizer/LLVMExtractor.hpp handsanitizer/LLVMExtractor.cpp handsanitizer/Module.cpp handsanitizer/Module.h)
target_sources(HandSan PRIVATE
        handsanitizer/main.cpp handsanitizer/name_generation.cpp handsanitizer/code_generation.cpp)


target_link_libraries(HandSan PRIVATE
        LLVM
        nlohmann_json nlohmann_json::nlohmann_json)
