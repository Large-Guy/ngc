#ifndef NGC_FLOAT_NODE_H
#define NGC_FLOAT_NODE_H
#include <variant>

#include "expression_node.h"


class FloatNode : public ExpressionNode {
public:
    FloatNode(double value, bool is_double);

    std::unique_ptr<AstNode> Clone() const override;

    bool is_double;
    double value;
};


#endif //NGC_FLOAT_NODE_H
