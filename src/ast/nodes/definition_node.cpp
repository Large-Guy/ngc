#include "definition_node.h"

#include <utility>

#include "../../memory_utils.h"

DefinitionNode::DefinitionNode(std::string name, std::unique_ptr<TypeNode> type) : name(std::move(name)),
    type(std::move(type)) {
}

std::unique_ptr<AstNode> DefinitionNode::Clone() const {
    return std::make_unique<DefinitionNode>(name, UniqueCast<TypeNode>(type->Clone()));
}
