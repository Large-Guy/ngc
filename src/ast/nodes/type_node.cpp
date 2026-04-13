#include "type_node.h"
#include "../../memory_utils.h"
#include "backends/backend.h"

TypeNode::TypeNode(TypeNodeType type, std::unique_ptr<TypeNode> subtype,
                   std::unique_ptr<ExpressionNode> capacity) : type(type), subtype(),
                                                               capacity(std::move(capacity)) {
    if (subtype != nullptr)
        this->subtype.push_back(std::move(subtype));
}

TypeNode::TypeNode(TypeNodeType type, std::vector<std::unique_ptr<TypeNode> > subtype,
                   std::unique_ptr<ExpressionNode> capacity) : type(type), subtype(std::move(subtype)),
                                                               capacity(std::move(capacity)) {
}

std::unique_ptr<AstNode> TypeNode::Clone() const {
    std::vector<std::unique_ptr<TypeNode> > types;

    for (auto& s: subtype) {
        types.push_back(UniqueCast<TypeNode>(s->Clone()));
    }
    return std::make_unique<TypeNode>(type, std::move(types),
                                      capacity ? UniqueCast<ExpressionNode>(capacity->Clone()) : nullptr);
}

bool TypeNode::Integer() const {
    return type >= TypeNodeType::BOOL && type <= TypeNodeType::U64;
}

bool TypeNode::Float() const {
    return type >= TypeNodeType::F32 && type <= TypeNodeType::F64;
}

bool TypeNode::Signed() const {
    return type >= TypeNodeType::I8 && type <= TypeNodeType::I64;
}

bool TypeNode::Boolean() const {
    return type == TypeNodeType::BOOL;
}

bool TypeNode::Pointer() const {
    return type == TypeNodeType::BORROW || type == TypeNodeType::OWNER;
}

bool TypeNode::Equal(const TypeNode* other, bool borrowConversion) const {
    if (other == nullptr || type != other->type) {
        if (!borrowConversion) {
            return false;
        }
        if (type != TypeNodeType::BORROW && type != TypeNodeType::OWNER || other->type != TypeNodeType::BORROW && other
            ->type !=
            TypeNodeType::OWNER) {
            return false;
        }
    }
    if (subtype.empty() || other->subtype.empty()) {
        return subtype.empty() && other->subtype.empty();
    }
    if (subtype.size() != other->subtype.size())
        return false;
    for (auto i = 0; i < subtype.size(); i++) {
        if (!subtype[i]->Equal(other->subtype[i].get(), borrowConversion))
            return false;
    }
    if (capacity != nullptr) {
        if (other->capacity != nullptr) {
            if (Backend::EvaluateInt(capacity.get()) !=
                Backend::EvaluateInt(other->capacity.get())) {
                return false;
            }
            return true;
        }
        return false;
    }
    return true; // TODO: handle capacity
}

bool TypeNode::Indexable() const {
    return type == TypeNodeType::ARRAY || type == TypeNodeType::MAP || type == TypeNodeType::TUPLE || type ==
           TypeNodeType::SIMD;
}
