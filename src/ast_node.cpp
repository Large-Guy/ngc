#include "ast_node.h"

#include<algorithm>

std::unique_ptr<AstNode> AstNode::Create(AstNodeType type, std::optional<Token> token) {
    auto node = std::unique_ptr<AstNode>(new AstNode(type, std::move(token)));
    return node;
}

AstNode::~AstNode() = default;

AstNode * AstNode::parent() const {
    return parent_;
}

AstNode * AstNode::operator[](size_t index) const {
    return children_[index].get();
}

void AstNode::AddNode(std::unique_ptr<AstNode> node) {
    if (node->parent_ != nullptr) {
        node->parent_->RemoveNode(node.get());
    }

    node->parent_ = this;
    children_.push_back(std::move(node));
}

void AstNode::RemoveNode(const AstNode *node) {
    for (auto i = 0; i < children_.size(); ++i) {
        if (children_[i].get() == node) {
            children_[i]->parent_ = nullptr;
            children_.erase(children_.begin() + i);
            return;
        }
    }
}

AstNode::AstNode(AstNodeType type, std::optional<Token> token) {
    this->type = type;
    this->token = std::move(token);
    parent_ = nullptr;
}
