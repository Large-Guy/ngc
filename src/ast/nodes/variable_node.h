#ifndef NGC_VARIABLE_NODE_H
#define NGC_VARIABLE_NODE_H
#include "definition_node.h"
#include "expression_node.h"
#include "type_node.h"


class VariableNode : public DefinitionNode {
public:
    std::unique_ptr<ExpressionNode> value;

    VariableNode(const std::string& name, std::unique_ptr<TypeNode> type,
                 std::unique_ptr<ExpressionNode> value = nullptr);

    [[nodiscard]] std::unique_ptr<AstNode> Clone() const override;
};


#endif //NGC_VARIABLE_NODE_H
