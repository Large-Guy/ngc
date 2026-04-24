#include "block.h"

#include <llvm/IR/BasicBlock.h>

Block::Block(llvm::LLVMContext& context, llvm::Function* function, const std::string& name) {
    basic_block = llvm::BasicBlock::Create(context, name, function);
}

bool Block::Valid(llvm::Value* value) const {
    // scan backwards, but avoid getting into a circle somehow
    std::unordered_set<const Block*> visited;
    return ValidHelper(value, visited);
}

void Block::Connect(const std::shared_ptr<Block>& successor) {
    successors.push_back(successor);
    successor->predecessors.push_back(weak_from_this());
}

void Block::Move(llvm::Value* value) {
    moved_values.emplace(value);
}

bool Block::ValidHelper(llvm::Value* value, std::unordered_set<const Block*>& visited) const {
    if (visited.contains(this)) {
        return true;
    }
    
    visited.insert(this);
    
    if (moved_values.contains(value)) {
        return false;
    }
    for (auto& pred : predecessors) {
        auto lock = pred.lock();
        if (!lock->ValidHelper(value, visited)) {
            return false;
        }
    }
    return true;
}
