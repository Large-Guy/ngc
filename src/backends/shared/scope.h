#ifndef NGC_SCOPE_H
#define NGC_SCOPE_H
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/JSON.h>

#include "block.h"
#include "../../ast/nodes/type_node.h"

class Scope {
public:
    void PushScope();

    void PopScope(struct LLVMBackend* backend, llvm::IRBuilder<>* builder, Block* block);

    void Declare(std::string name, llvm::Value* value, std::unique_ptr<TypeNode> type);

    llvm::Value* Lookup(const std::string& name);

    TypeNode* Type(const std::string& name);

private:
    void Free(struct LLVMBackend* backend, llvm::IRBuilder<>* builder, std::pair<llvm::Value*, std::unique_ptr<TypeNode>>& value);
    
    size_t anonymous = 0;
    std::vector<std::unordered_map<std::string, std::pair<llvm::Value*, std::unique_ptr<TypeNode>>>> stack;
};


#endif //NGC_SCOPE_H