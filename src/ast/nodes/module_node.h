#ifndef NGC_MODULE_NODE_H
#define NGC_MODULE_NODE_H
#include <string>
#include <vector>

#include "definition_node.h"
#include "statement_node.h"
#include "../ast_node.h"


class ModuleNode : public DefinitionNode {
public:
    ModuleNode(std::string name);

    std::unique_ptr<AstNode> Clone() const override;

    ModuleNode* GetSubmodule(std::string name) const;

    std::vector<std::unique_ptr<StatementNode> > statements;
};


#endif //NGC_MODULE_NODE_H
