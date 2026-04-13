#ifndef NGC_TYPE_SCOPE_H
#define NGC_TYPE_SCOPE_H
#include <utility>
#include <vector>

#include "ast/nodes/type_node.h"


class TypeScope {
public:
    std::string name;
    std::vector<std::unique_ptr<TypeScope>> children;
    
    std::unique_ptr<TypeNode> type;
    
    TypeScope* parent;
    
    TypeScope(std::string name, TypeScope* parent, std::unique_ptr<TypeNode> type = nullptr);

    TypeScope* Declare(const std::string& name, std::unique_ptr<TypeNode> type = nullptr);

    TypeScope* LookupSubtype(const std::string& name) const;
    
    TypeScope* Lookup(const std::string& name) const;
};


#endif //NGC_TYPE_SCOPE_H