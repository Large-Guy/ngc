#ifndef NGC_BINARYNODE_H
#define NGC_BINARYNODE_H
#include <memory>

#include "expression_node.h"
#include "../ast_node.h"

enum class BinaryNodeType {
    ADD, SUBTRACT, MULTIPLY, DIVIDE,
    EXPONENT, MODULO,
    BITWISE_OR, BITWISE_XOR, BITWISE_AND, BITWISE_LEFT, BITWISE_RIGHT,
    GREATER, GREATER_EQUAL, LESS, LESS_EQUAL, EQUAL, NOT_EQUAL, AND, OR,
};

class BinaryNode : public ExpressionNode {
public:
    BinaryNode(BinaryNodeType type, std::unique_ptr<ExpressionNode> left, std::unique_ptr<ExpressionNode> right);

    [[nodiscard]] std::unique_ptr<AstNode> Clone() const override;

    BinaryNodeType type;
    std::unique_ptr<ExpressionNode> left;
    std::unique_ptr<ExpressionNode> right;
};


#endif //NGC_BINARYNODE_H
