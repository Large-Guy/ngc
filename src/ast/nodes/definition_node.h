#ifndef NGC_DEFINITION_NODE_H
#define NGC_DEFINITION_NODE_H
#include <string>

#include "statement_node.h"
#include "type_node.h"
#include "../ast_node.h"


class DefinitionNode : public StatementNode {
public:
    DefinitionNode(std::string name, std::unique_ptr<TypeNode> type);

    std::unique_ptr<AstNode> Clone() const override;

    std::string name;
    std::unique_ptr<TypeNode> type;
};


#endif //NGC_DEFINITION_NODE_H
