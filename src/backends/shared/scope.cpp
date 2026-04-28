#include "scope.h"

#include "memory_utils.h"
#include "backends/llvm_backend.h"

void Scope::PushScope() {
    stack.push_back({});
}

void Scope::PopScope(struct LLVMBackend* backend, llvm::IRBuilder<>* builder, Block* block) {
    auto& top = stack.back();
    
    for (auto& pair : top) {
        auto& val = pair.second;
        if (!block->Valid(val.first))
            continue;
        if (val.second->type == TypeNodeType::OWNER) {
            Free(backend, builder, val);
        }
    }
    
    stack.pop_back();
}

void Scope::Declare(std::string name, llvm::Value* value, std::unique_ptr<TypeNode> type) {
    if (name.empty())
        name = std::to_string(anonymous++);
    stack.back()[name] = std::pair(value, std::move(type));
}

llvm::Value* Scope::Lookup(const std::string& name) {
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second.first;
        }
    }
    return nullptr;
}

TypeNode* Scope::Type(const std::string& name) {
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second.second.get();
        }
    }
    return nullptr;
}

void Scope::Free(LLVMBackend* backend, llvm::IRBuilder<>* builder, std::pair<llvm::Value*, std::unique_ptr<TypeNode>>& value) {
    if (value.second->subtype[0]->type == TypeNodeType::OWNER) {
        //load and free the subptr
        auto subtype = UniqueCast<TypeNode>(value.second->subtype[0]->Clone());
        auto load = builder->CreateLoad(backend->GenerateType(subtype.get()), value.first);
        auto free = std::pair(static_cast<Value*>(load), std::move(subtype));
        Free(backend, builder, free);
    }
    if (isa<AllocaInst>(value.first))
        return; // will be cleaned up by the stack
    builder->CreateFree(value.first);
}
