#include "type_scope.h"

TypeScope::TypeScope(std::string name, TypeScope* parent, std::unique_ptr<TypeNode> type): name(std::move(name)), parent(parent), type(std::move(type)) {
}

TypeScope* TypeScope::Declare(const std::string& name, std::unique_ptr<TypeNode> type) {
    auto child = std::make_unique<TypeScope>(name, this, std::move(type));
    auto ptr = child.get();
    children.push_back(std::move(child));
    return ptr;
}

TypeScope* TypeScope::LookupSubtype(const std::string& name) const {
    for (const auto & it : children) {
        if (it->name == name) {
            return it.get();
        }
    }
    return nullptr;
}

TypeScope* TypeScope::Lookup(const std::string& name) const {
    if (auto lookup = LookupSubtype(name)) {
        return lookup;
    }
    return parent->LookupSubtype(name);
}
