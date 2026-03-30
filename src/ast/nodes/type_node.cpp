#include "type_node.h"
#include "../../memory_utils.h"

TypeNode::TypeNode(TypeNodeType type, std::unique_ptr<TypeNode> subtype,
                   std::unique_ptr<ExpressionNode> capacity) : type(type), subtype(std::move(subtype)),
                                                               capacity(std::move(capacity)) {
}

std::unique_ptr<AstNode> TypeNode::Clone() const {
    std::unique_ptr<TypeNode> sub = nullptr;
    if (subtype != nullptr) {
        sub = UniqueCast<TypeNode>(subtype->Clone());
    }
    return std::make_unique<TypeNode>(type, std::move(sub),
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

size_t TypeNode::Size() const {
    switch (type) {
        case TypeNodeType::BOOL:
            return 1;
        case TypeNodeType::VOID:
            return 0;
        case TypeNodeType::BORROW:
            return 0;
        case TypeNodeType::OWNER:
            return 0;
        case TypeNodeType::FUNCTION:
            return 0;
        case TypeNodeType::OPTIONAL:
            return 0;
        case TypeNodeType::ARRAY:
            return 0;
        case TypeNodeType::MAP:
            return 0;
        case TypeNodeType::SIMD:
            return 0;
        case TypeNodeType::TUPLE:
            return 0;
        case TypeNodeType::I8:
            return 1;
        case TypeNodeType::I16:
            return 2;
        case TypeNodeType::I32:
            return 4;
        case TypeNodeType::I64:
            return 8;
        case TypeNodeType::U8:
            return 1;
        case TypeNodeType::U16:
            return 2;
        case TypeNodeType::U32:
            return 4;
        case TypeNodeType::U64:
            return 8;
        case TypeNodeType::F32:
            return 4;
        case TypeNodeType::F64:
            return 8;
        case TypeNodeType::TYPE_COUNT:
            return 0;
        case TypeNodeType::INFERED:
            return 0;
    }
    return 0;
}

bool TypeNode::Equal(const TypeNode* other) const {
    if (other == nullptr || type != other->type) {
        return false;
    }
    if (subtype == nullptr || other->subtype == nullptr) {
        return subtype == nullptr && other->subtype == nullptr;
    }
    return subtype->Equal(other->subtype.get()); // TODO: handle capacity
}
