#ifndef NGC_STRUCT_NODE_H
#define NGC_STRUCT_NODE_H
#include <unordered_map>

#include "definition_node.h"
#include "statement_node.h"
#include "type_node.h"


class StructNode : public DefinitionNode {
public:
    StructNode(std::string name, std::unique_ptr<TypeNode> type, std::vector<std::string> field_names);

    std::unique_ptr<AstNode> Clone() const override;

    std::vector<std::string> field_names;
};


#endif //NGC_STRUCT_NODE_H