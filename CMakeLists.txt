cmake_minimum_required(VERSION 4.1)
project(ngc)

set(CMAKE_CXX_STANDARD 20)

add_executable(ngc src/main.cpp
        src/lexer.cpp
        src/lexer.h
        src/ast_node.cpp
        src/ast_node.h
        src/parser.cpp
        src/parser.h)
