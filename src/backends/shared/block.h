#ifndef NGC_BLOCK_H
#define NGC_BLOCK_H
#include <unordered_set>
#include <llvm/IR/Value.h>


class Block : public std::enable_shared_from_this<Block> {
public:
    llvm::BasicBlock* basic_block;
    
    Block(llvm::LLVMContext& context, llvm::Function* function = nullptr, const std::string& name = "");
    
    bool Valid(llvm::Value* value) const;
    
    void Connect(const std::shared_ptr<Block>& successor);
    
    void Move(llvm::Value* value);
    
private:
    bool ValidHelper(llvm::Value* value, std::unordered_set<const Block*>& visited) const;
    
    std::vector<std::weak_ptr<Block>> predecessors;
    std::vector<std::shared_ptr<Block>> successors;
    
    std::unordered_set<llvm::Value*> moved_values;
};



#endif //NGC_BLOCK_H
