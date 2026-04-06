#include "backend.h"

#include <cmath>

#include "ast/nodes/binary_node.h"
#include "ast/nodes/float_node.h"
#include "ast/nodes/integer_node.h"

int64_t Backend::EvaluateInt(ExpressionNode* unique) const {
    if (auto integer = is<IntegerNode>(unique)) {
        return integer->value;
    }
    if (auto floating = is<FloatNode>(unique)) {
        throw std::runtime_error("Expected integer");
    }
    if (auto binary = is<BinaryNode>(unique)) {
        auto left = EvaluateInt(binary->left.get());
        auto right = EvaluateInt(binary->right.get());
        switch (binary->type) {
            case BinaryNodeType::ADD:
                return left + right;
            case BinaryNodeType::SUBTRACT:
                return left - right;
            case BinaryNodeType::MULTIPLY:
                return left * right;
            case BinaryNodeType::DIVIDE:
                return left / right;
            case BinaryNodeType::EXPONENT:
                return static_cast<int64_t>(std::pow(static_cast<int64_t>(left), static_cast<int64_t>(right)));
            case BinaryNodeType::MODULO:
                return left % right;
            case BinaryNodeType::BITWISE_OR:
                return left | right;
            case BinaryNodeType::BITWISE_XOR:
                return left ^ right;
            case BinaryNodeType::BITWISE_AND:
                return left & right;
            case BinaryNodeType::BITWISE_LEFT:
                return left << right;
            case BinaryNodeType::BITWISE_RIGHT:
                return left >> right;
            case BinaryNodeType::GREATER:
                return left > right ? 1 : 0;
            case BinaryNodeType::GREATER_EQUAL:
                return left >= right ? 1 : 0;
            case BinaryNodeType::LESS:
                return left < right ? 1 : 0;
            case BinaryNodeType::LESS_EQUAL:
                return left <= right ? 1 : 0;
            case BinaryNodeType::EQUAL:
                return left == right ? 1 : 0;
            case BinaryNodeType::NOT_EQUAL:
                return left != right ? 1 : 0;
            case BinaryNodeType::AND:
                return left && right ? 1 : 0;
            case BinaryNodeType::OR:
                return left || right ? 1 : 0;
        }
    }
    throw std::runtime_error("Unsupported operator in const expression");
}


size_t Backend::EvaluateSize(const TypeNode* type_node) const {
    switch (type_node->type) {
        case TypeNodeType::BOOL:
            return 1;
        case TypeNodeType::VOID:
            return 0;
        case TypeNodeType::BORROW:
            return sizeof(size_t);
        case TypeNodeType::OWNER:
            return sizeof(size_t);
        case TypeNodeType::FUNCTION:
            return sizeof(size_t);
        case TypeNodeType::OPTIONAL:
            throw std::runtime_error("Optional types are not supported");
        case TypeNodeType::ARRAY:
            throw std::runtime_error("Array types are not supported");
        case TypeNodeType::MAP:
            throw std::runtime_error("Map types are not supported");
        case TypeNodeType::SIMD:
            return EvaluateSize(type_node->subtype[0].get()) * EvaluateInt(type_node->capacity.get());
        case TypeNodeType::TUPLE:
            throw std::runtime_error("Tuple types are not supported");
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
    }
    return 0;
}