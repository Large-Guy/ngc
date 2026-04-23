#include "type_node.h"

#include <utility>

#include "tuple_node.h"
#include "../../memory_utils.h"
#include "backends/backend.h"

TypeNode::TypeNode(TypeNodeType type, std::unique_ptr<TypeNode> subtype,
                   std::unique_ptr<ExpressionNode> capacity, std::string name) : type(type), subtype(),
                                                               capacity(std::move(capacity)), name(std::move(name)) {
    if (subtype != nullptr)
        this->subtype.push_back(std::move(subtype));
}

TypeNode::TypeNode(TypeNodeType type, std::vector<std::unique_ptr<TypeNode> > subtype,
                   std::unique_ptr<ExpressionNode> capacity, std::string name) : type(type), subtype(std::move(subtype)),
                                                               capacity(std::move(capacity)), name(std::move(name)) {
}

std::unique_ptr<AstNode> TypeNode::Clone() const {
    std::vector<std::unique_ptr<TypeNode> > types;

    for (auto& s: subtype) {
        types.push_back(UniqueCast<TypeNode>(s->Clone()));
    }
    return std::make_unique<TypeNode>(type, std::move(types),
                                      capacity ? UniqueCast<ExpressionNode>(capacity->Clone()) : nullptr, name);
}

bool TypeNode::HasField(const std::string& name) const {
    for (auto& s: subtype) {
        if (s->name == name) {
            return true;
        }
    }
    return false;
}

int TypeNode::GetFieldIndex(const std::string& name) const {
    for (auto i = 0; i < subtype.size(); i++) {
        if (subtype[i]->name == name) {
            return i;
        }
    }
    return -1;
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
            if (auto tuple = dynamic_cast<TupleNode*>(capacity.get())) {
                //compare elements
                auto other_tuple = dynamic_cast<TupleNode*>(other->capacity.get());
                if (other_tuple == nullptr)
                    return false;
                if (tuple->elements.size() != other_tuple->elements.size())
                    return false;
                for (auto i = 0; i < tuple->elements.size(); i++) {
                    if (Backend::EvaluateInt(tuple->elements[i].get()) !=
                        Backend::EvaluateInt(other_tuple->elements[i].get())) {
                        return false;
                    }
                }
            }
            else {
                if (Backend::EvaluateInt(capacity.get()) !=
                    Backend::EvaluateInt(other->capacity.get())) {
                    return false;
                    }
            }
        }
        else {
            return false;
        }
    }
    return true; // TODO: handle capacity
}

bool TypeNode::Indexable() const {
    return type == TypeNodeType::ARRAY || type == TypeNodeType::MAP || type == TypeNodeType::TUPLE || type ==
           TypeNodeType::SIMD || type == TypeNodeType::TENSOR;
}

bool TypeNode::Vectorizable() const {
    switch (type) {
        case TypeNodeType::VOID:
        case TypeNodeType::STRUCT:
        case TypeNodeType::OPTIONAL:
        case TypeNodeType::ARRAY:
        case TypeNodeType::MAP:
        case TypeNodeType::TUPLE:
        case TypeNodeType::SIMD:
        case TypeNodeType::TENSOR:
            return false;
        default:
            return true;
    }
}
