cmake_minimum_required(VERSION 3.19)
project(HandSanitizer)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM 10 REQUIRED)
add_definitions(${LLVM_DEFINITIONS})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

find_package(nlohmann_json CONFIG REQUIRED)

set(INCLUDE_DIR include)

add_executable(HandSanitizer handsan.cpp LLVMExtractor.hpp LLVMExtractor.cpp Module.cpp Module.h)
target_sources(HandSanitizer PRIVATE
        main.cpp name_generation.cpp code_generation.cpp ${INCLUDE_DIR}/code_generation.hpp ${INCLUDE_DIR}/code_generation.hpp ${INCLUDE_DIR}/handsan.hpp)


target_link_libraries(HandSanitizer PRIVATE
        LLVM
        nlohmann_json nlohmann_json::nlohmann_json)
