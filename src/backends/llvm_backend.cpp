#include "llvm_backend.h"

#include <iostream>

#include "../ast/nodes/function_node.h"
#include "../ast/nodes/module_node.h"

void LLVMBackend::Generate(std::vector<std::unique_ptr<AstNode> > nodes) {
    context_ = std::make_unique<LLVMContext>();
    builder_ = std::make_unique<IRBuilder<> >(*context_);

    for (const auto& node: nodes) {
        if (const auto module = is<ModuleNode>(node.get())) {
            module_ = std::make_unique<Module>(module->path, *context_);
            std::cout << "Module: " << module->path << std::endl;
            continue;
        }
        if (const auto function = is<FunctionNode>(node.get())) {
            GenerateFunction(function);
        }
    }
}

void LLVMBackend::GenerateFunction(FunctionNode* function) {
    std::cout << "Function: " << function->name << std::endl;
}
