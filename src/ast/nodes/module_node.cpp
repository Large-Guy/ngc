#include "module_node.h"

#include "../../memory_utils.h"

ModuleNode::ModuleNode(std::string path) : path(std::move(path)) {
}

std::unique_ptr<AstNode> ModuleNode::Clone() const {
    auto mod = new ModuleNode(path);

    mod->statements.reserve(statements.size());
    for (auto& statement: statements) {
        mod->statements.push_back(UniqueCast<StatementNode>(statement->Clone()));
    }

    return std::unique_ptr<AstNode>(mod);
}

ModuleNode* ModuleNode::GetSubmodule(std::string name) const {
    for (const auto& statement: statements) {
        if (const auto module = dynamic_cast<ModuleNode*>(statement.get())) {
            if (module->path == name) {
                return module;
            }
        }
    }
    return nullptr;
}
