#ifndef NGC_LLVM_BACKEND_H
#define NGC_LLVM_BACKEND_H
#include "backend.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

using namespace llvm;

class LLVMBackend : public Backend {
public:
    void Generate(std::vector<std::unique_ptr<AstNode> > nodes) override;

    int64_t Evaluate(ExpressionNode* unique);

    Type* GenerateType(TypeNode* type);

    Value * GenerateExpression(ExpressionNode * get);

    void GenerateStatement(StatementNode * get);

    void GenerateFunction(FunctionNode* function);

private:
    std::unique_ptr<LLVMContext> context_;
    std::unique_ptr<IRBuilder<> > builder_;
    std::unique_ptr<Module> module_;

    BasicBlock* block_ = nullptr;
};


#endif //NGC_LLVM_BACKEND_H
