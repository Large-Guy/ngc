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
    if (subtype == nullptr || other->subtype == nullptr) {
        return subtype == nullptr && other->subtype == nullptr;
    }
    return subtype->Equal(other->subtype.get(), borrowConversion); // TODO: handle capacity
}

bool TypeNode::Indexable() {
    return type == TypeNodeType::ARRAY || type == TypeNodeType::MAP || type == TypeNodeType::TUPLE || type == TypeNodeType::SIMD;
}
