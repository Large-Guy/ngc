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

    std::pair<Value *, std::unique_ptr<TypeNode>> Drill(std::pair<Value *, std::unique_ptr<TypeNode>> value);

    std::pair<Value*, std::unique_ptr<TypeNode>> Cast(std::pair<Value*, std::unique_ptr<TypeNode>> value, TypeNode* type);

    std::pair<Value *, std::unique_ptr<TypeNode>> GenerateRValue(AstNode *get);

    std::pair<Value *, std::unique_ptr<TypeNode>> GenerateLValue(AstNode *get);

    void GenerateFunction(FunctionNode* function);

private:
    std::unique_ptr<LLVMContext> context_;
    std::unique_ptr<IRBuilder<> > builder_;
    std::unique_ptr<Module> module_;

    Scope<Value> scope_;

    Function* func = nullptr;
};


#endif //NGC_LLVM_BACKEND_H
