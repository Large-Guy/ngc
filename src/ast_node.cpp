#include "ast_node.h"

#include<algorithm>
#include <iostream>

const char* AstNodeTypeToString(AstNodeType e) {
    switch (e) {
        case AstNodeType::_NULL: return "NULL";
        case AstNodeType::VOID: return "VOID";
        case AstNodeType::BORROW: return "BORROW";
        case AstNodeType::OWNER: return "OWNER";
        case AstNodeType::OPTIONAL: return "OPTIONAL";
        case AstNodeType::ARRAY: return "ARRAY";
        case AstNodeType::MAP: return "MAP";
        case AstNodeType::SIMD: return "SIMD";
        case AstNodeType::BOOL: return "BOOL";
        case AstNodeType::I8: return "I8";
        case AstNodeType::I16: return "I16";
        case AstNodeType::I32: return "I32";
        case AstNodeType::I64: return "I64";
        case AstNodeType::U8: return "U8";
        case AstNodeType::U16: return "U16";
        case AstNodeType::U32: return "U32";
        case AstNodeType::U64: return "U64";
        case AstNodeType::F32: return "F32";
        case AstNodeType::F64: return "F64";
        case AstNodeType::TYPE_COUNT: return "TYPE_COUNT";
        case AstNodeType::INFER: return "INFER";
        case AstNodeType::SEQUENCE: return "SEQUENCE";
        case AstNodeType::NAME: return "NAME";
        case AstNodeType::MODULE: return "MODULE";
        case AstNodeType::GET: return "GET";
        case AstNodeType::FUNCTION: return "FUNCTION";
        case AstNodeType::VARIABLE: return "VARIABLE";
        case AstNodeType::RETURN: return "RETURN";
        case AstNodeType::IF: return "IF";
        case AstNodeType::FLOAT: return "FLOAT";
        case AstNodeType::INT: return "INT";
        case AstNodeType::TRUE: return "TRUE";
        case AstNodeType::FALSE: return "FALSE";
            
        case AstNodeType::NEGATE: return "NEGATE";
        case AstNodeType::ADD: return "ADD";
        case AstNodeType::SUBTRACT: return "SUBTRACT";
        case AstNodeType::MULTIPLY: return "MULTIPLY";
        case AstNodeType::DIVIDE: return "DIVIDE";

        case AstNodeType::EXPONENT: return "EXPONENT";
        case AstNodeType::BITWISE_AND: return "AND";
        case AstNodeType::BITWISE_OR: return "OR";
        case AstNodeType::BITWISE_XOR: return "XOR";
        case AstNodeType::MODULO: return "MODULO";
        case AstNodeType::SHIFT_LEFT: return "SHIFT_LEFT";
        case AstNodeType::SHIFT_RIGHT: return "SHIFT_RIGHT";
        case AstNodeType::GREATER: return "GREATER";
        case AstNodeType::GREATER_EQUAL: return "GREATER_EQUAL";
        case AstNodeType::LESS: return "LESS";
        case AstNodeType::LESS_EQUAL: return "LESS_EQUAL";
        case AstNodeType::EQUAL: return "EQUAL";
        case AstNodeType::NOT_EQUAL: return "NOT_EQUAL";

        case AstNodeType::AND: return "AND";
        case AstNodeType::OR: return "OR";

        case AstNodeType::NOT: return "NOT";
        case AstNodeType::ADDRESS: return "ADDRESS";
        case AstNodeType::LOCK: return "LOCK";
            
        case AstNodeType::ASSIGN: return "ASSIGN";
        case AstNodeType::CAST: return "CAST";
        case AstNodeType::REINTERPRET: return "REINTERPRET";
        case AstNodeType::HEAP: return "HEAP";

        case AstNodeType::LIST: return "LIST";

        case AstNodeType::INDEX: return "INDEX";
        case AstNodeType::CALL: return "CALL";
            
        default: return "unknown";
    }
}

std::unique_ptr<AstNode> AstNode::New(AstNodeType type, std::optional<Token> token) {
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

std::unique_ptr<AstNode> AstNode::Clone() {
    auto node = std::unique_ptr<AstNode>(new AstNode(type, token));
    for (const auto & child : children_) {
        node->AddNode(child->Clone());
    }
    return node;
}

void AstNode::Debug(uint32_t depth) const {
    for (auto i = 0; i < depth; ++i) {
        std::cout << "|   ";
    }
    std::cout << AstNodeTypeToString(type);
    if (token) {
        std::cout << " (" << token.value().value << ")";
    }
    std::cout << std::endl;
    for (const auto & child : children_) {
        child->Debug(depth + 1);
    }
}

AstNode::AstNode(AstNodeType type, std::optional<Token> token) {
    this->type = type;
    this->token = std::move(token);
    parent_ = nullptr;
}
