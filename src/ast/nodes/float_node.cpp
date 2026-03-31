#include "float_node.h"

FloatNode::FloatNode(double value, bool is_double) : is_double(is_double), value(value) {
}

std::unique_ptr<AstNode> FloatNode::Clone() const {
    return std::make_unique<FloatNode>(value, is_double);
}
