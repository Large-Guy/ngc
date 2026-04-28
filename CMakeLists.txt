cmake_minimum_required(VERSION 4.1)
project(ngc)

set(CMAKE_CXX_STANDARD 20)

add_executable(ngc src/main.cpp
        src/lexer.cpp
        src/lexer.h
        src/parser.cpp
        src/parser.h
        src/backends/backend.cpp
        src/backends/backend.h
        src/ast/nodes/type_node.cpp
        src/ast/nodes/type_node.h
        src/ast/nodes/binary_node.cpp
        src/ast/nodes/binary_node.h
        src/ast/ast_node.cpp
        src/ast/ast_node.h
        src/ast/nodes/expression_node.cpp
        src/ast/nodes/expression_node.h
        src/ast/nodes/definition_node.cpp
        src/ast/nodes/definition_node.h
        src/ast/nodes/unary_node.cpp
        src/ast/nodes/unary_node.h
        src/ast/nodes/integer_node.cpp
        src/ast/nodes/integer_node.h
        src/ast/nodes/float_node.cpp
        src/ast/nodes/float_node.h
        src/ast/nodes/compound_statement.cpp
        src/ast/nodes/compound_statement.h
        src/ast/nodes/statement_node.cpp
        src/ast/nodes/statement_node.h
        src/ast/nodes/cast_node.cpp
        src/ast/nodes/cast_node.h
        src/ast/nodes/function_node.cpp
        src/ast/nodes/function_node.h
        src/ast/nodes/variable_node.cpp
        src/ast/nodes/variable_node.h
        src/ast/nodes/tuple_node.cpp
        src/ast/nodes/tuple_node.h
        src/ast/nodes/lambda_node.cpp
        src/ast/nodes/lambda_node.h
        src/ast/nodes/call_node.cpp
        src/ast/nodes/call_node.h
        src/ast/nodes/module_node.cpp
        src/ast/nodes/module_node.h
        src/ast/nodes/import_node.cpp
        src/ast/nodes/import_node.h
        src/ast/nodes/assign_node.cpp
        src/ast/nodes/assign_node.h
        src/ast/nodes/heap_node.cpp
        src/ast/nodes/heap_node.h
        src/ast/nodes/field_node.cpp
        src/ast/nodes/field_node.h
        src/ast/nodes/identifier_node.cpp
        src/ast/nodes/identifier_node.h
        src/ast/nodes/literal_node.cpp
        src/ast/nodes/literal_node.h
        src/memory_utils.cpp
        src/memory_utils.h
        src/ast/nodes/index_node.cpp
        src/ast/nodes/index_node.h
        src/ast/nodes/return_node.cpp
        src/ast/nodes/return_node.h
        src/ast/nodes/while_node.cpp
        src/ast/nodes/while_node.h
        src/ast/nodes/if_node.cpp
        src/ast/nodes/if_node.h
        src/ast/nodes/for_node.cpp
        src/ast/nodes/for_node.h
        src/ast/nodes/do_while_node.cpp
        src/ast/nodes/do_while_node.h
        src/backends/llvm_backend.cpp
        src/backends/llvm_backend.h
        src/backends/shared/scope.cpp
        src/backends/shared/scope.h
        src/ast/nodes/AddressNode.cpp
        src/ast/nodes/AddressNode.h
        src/ast/nodes/LockNode.cpp
        src/ast/nodes/LockNode.h
        src/ast/nodes/struct_node.cpp
        src/ast/nodes/struct_node.h
        src/type_scope.cpp
        src/type_scope.h
        src/backends/shared/block.cpp
        src/backends/shared/block.h
)

find_package(LLVM 18.1.8 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

target_include_directories(ngc PRIVATE ${LLVM_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(ngc PRIVATE ${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(LLVM_LIBS
        core
        support
        irreader
        x86codegen
        x86asmparser
        passes
        aarch64codegen
        aarch64asmparser
        aarch64desc
        aarch64info
)

target_link_libraries(ngc PRIVATE ${LLVM_LIBS})