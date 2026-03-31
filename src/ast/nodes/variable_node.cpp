#include "variable_node.h"

#include "../../memory_utils.h"

VariableNode::VariableNode(const std::string& name, std::unique_ptr<TypeNode> type,
                           std::unique_ptr<ExpressionNode> value) : DefinitionNode(name, std::move(type)),

                                                                    value(std::move
                                                                        (value)
                                                                    ) {
}

std::unique_ptr<AstNode> VariableNode::Clone() const {
    return std::make_unique<VariableNode>(name, UniqueCast<TypeNode>(type->Clone()),
                                          UniqueCast<ExpressionNode>(value->Clone()));
}

