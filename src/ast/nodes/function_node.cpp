#include "function_node.h"
#include "../../memory_utils.h"

#include <utility>

FunctionNode::FunctionNode(const std::string& name, std::unique_ptr<TypeNode> return_type,
                           std::vector<std::unique_ptr<DefinitionNode> > args,
                           std::unique_ptr<StatementNode> body) : DefinitionNode(name, std::move(return_type)),
                                                                  args(std::move(args)), body(std::move(body)) {
}

std::unique_ptr<AstNode> FunctionNode::Clone() const {
    std::vector<std::unique_ptr<DefinitionNode> > arguments(args.capacity());
    for (auto& arg: args) {
        arguments.push_back(UniqueCast<DefinitionNode>(arg->Clone()));
    }

    return std::make_unique<FunctionNode>(name, UniqueCast<TypeNode>(type->Clone()), std::move(arguments),
                                          UniqueCast<StatementNode>(body->Clone()));
}



