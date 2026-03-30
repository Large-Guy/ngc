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

    Type* GenerateType(const TypeNode* type);

    std::pair<Value*, std::unique_ptr<TypeNode> > Drill(std::pair<Value*, std::unique_ptr<TypeNode> > value,
                                                        const TypeNode* type);

    std::pair<Value*, std::unique_ptr<TypeNode> > Cast(std::pair<Value*, std::unique_ptr<TypeNode> > value,
                                                       const TypeNode* type);

    std::unique_ptr<TypeNode> Promote(std::pair<Value*, std::unique_ptr<TypeNode> >& a,
                                      std::pair<Value*, std::unique_ptr<TypeNode> >& b);

    std::pair<Value*, std::unique_ptr<TypeNode> > GenerateRValue(AstNode* get, const TypeNode* expected);

    std::pair<Value*, std::unique_ptr<TypeNode> > GenerateLValue(AstNode* get);

    void GeneratePrototype(FunctionNode* function);

    void GenerateFunction(FunctionNode* function);

private:
    std::unique_ptr<LLVMContext> context_;
    std::unique_ptr<IRBuilder<> > builder_;
    std::unique_ptr<Module> module_;

    Scope<Value> scope_;

    Function* func = nullptr;
    BasicBlock* exit = nullptr;
    Value* ret = nullptr;
};


#endif //NGC_LLVM_BACKEND_H
