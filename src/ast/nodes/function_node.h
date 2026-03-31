#ifndef NGC_FUNCTION_NODE_H
#define NGC_FUNCTION_NODE_H
#include <memory>
#include <string>
#include <vector>

#include "definition_node.h"
#include "statement_node.h"
#include "type_node.h"


class FunctionNode : public DefinitionNode {
public:
    FunctionNode(const std::string& name, std::unique_ptr<TypeNode> return_type,
                 std::vector<std::unique_ptr<DefinitionNode> > args, std::unique_ptr<StatementNode> body);

    std::unique_ptr<AstNode> Clone() const override;

    std::vector<std::unique_ptr<DefinitionNode> > args;

    std::unique_ptr<StatementNode> body;
};

#endif //NGC_FUNCTION_NODE_H
