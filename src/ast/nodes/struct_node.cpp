#include "struct_node.h"

#include "memory_utils.h"


StructNode::StructNode(std::string name, std::unique_ptr<TypeNode> type, std::vector<std::string> field_names) : DefinitionNode(name, std::move(type)), field_names(std::move(field_names)) {
}

std::unique_ptr<AstNode> StructNode::Clone() const {
    return std::make_unique<StructNode>(name, UniqueCast<TypeNode>(type->Clone()), field_names);
}
