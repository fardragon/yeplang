cmake_minimum_required(VERSION 3.20)
project(yeplang)

set(CMAKE_CXX_STANDARD 20)

add_executable(yeplang src/main.cpp src/Tokenizer.cpp src/Parser.cpp src/Expression.cpp src/Types.cpp src/Function.cpp src/Generator.cpp src/Validator.cpp)

target_include_directories(yeplang PRIVATE include)

target_link_libraries(yeplang PRIVATE LLVM-14)

target_compile_options(yeplang PRIVATE -Wall -Wextra)
