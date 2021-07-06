cmake_minimum_required(VERSION 3.19)
project(HandSanitizer)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM 10 REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(HandSanitizer
        main.cpp)

target_link_libraries(HandSanitizer PRIVATE
        LLVM
        nlohmann_json nlohmann_json::nlohmann_json)
