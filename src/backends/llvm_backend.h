#ifndef NGC_LLVM_BACKEND_H
#define NGC_LLVM_BACKEND_H
#include "backend.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

#include "shared/Scope.h"

using namespace llvm;

class LLVMBackend : public Backend {
public:
    void Generate(std::vector<std::unique_ptr<AstNode> > nodes) override;

    int64_t Evaluate(ExpressionNode* unique);

    Type* GenerateType(TypeNode* type);

    Value * GenerateRValue(AstNode * get);
    Value* GenerateLValue(AstNode* get);

    void GenerateFunction(FunctionNode* function);

private:
    std::unique_ptr<LLVMContext> context_;
    std::unique_ptr<IRBuilder<> > builder_;
    std::unique_ptr<Module> module_;

    Scope<Value> scope_;

    BasicBlock* block_ = nullptr;
};


#endif //NGC_LLVM_BACKEND_H
