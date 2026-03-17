#ifndef NGC_LLVM_H
#define NGC_LLVM_H
#include "backend.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

using namespace llvm;

class LlvmBackend : Backend {
public:
    Type * CreateType(AstNode * ast_node);

    void Block(Function *function, AstNode *body);

    void Declaration(const AstNode & node);

    void Generate(std::vector<std::unique_ptr<AstNode>> nodes) override;

private:
    std::unique_ptr<LLVMContext> context_;
    std::unique_ptr<IRBuilder<>> builder_;
    std::unique_ptr<Module> module_;
};

#endif //NGC_LLVM_H
