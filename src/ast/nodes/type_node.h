#ifndef NGC_TYPENODE_H
#define NGC_TYPENODE_H
#include <memory>
#include <vector>

#include "expression_node.h"
#include "../ast_node.h"

enum class TypeNodeType {
    VOID,
    STRUCT,
    BORROW,
    OWNER,
    OPTIONAL,
    ARRAY,
    MAP,
    SIMD,
    MATRIX,
    TUPLE,
    FUNCTION,

    BOOL,

    I8,
    I16,
    I32,
    I64,

    U8,
    U16,
    U32,
    U64,

    F32,
    F64,

    TYPE_COUNT, // used for implicit cast table
};

class TypeNode : public AstNode {
public:
    TypeNode(TypeNodeType type, std::unique_ptr<TypeNode> subtype = nullptr,
             std::unique_ptr<ExpressionNode> capacity = nullptr, std::string name = "");

    TypeNode(TypeNodeType type, std::vector<std::unique_ptr<TypeNode>> subtype,
        std::unique_ptr<ExpressionNode> capacity = nullptr, std::string name = "");

    [[nodiscard]] std::unique_ptr<AstNode> Clone() const override;

    [[nodiscard]] bool HasField(const std::string& name) const;
    
    [[nodiscard]] int GetFieldIndex(const std::string& name) const;

    [[nodiscard]] bool Integer() const;

    [[nodiscard]] bool Float() const;

    [[nodiscard]] bool Signed() const;

    [[nodiscard]] bool Boolean() const;

    [[nodiscard]] bool Pointer() const;

    bool Equal(const TypeNode* other, bool borrowConversion) const;

    [[nodiscard]] bool Indexable() const;

    std::string name;
    TypeNodeType type;
    std::vector<std::unique_ptr<TypeNode>> subtype;
    std::unique_ptr<ExpressionNode> capacity;
};


#endif //NGC_TYPENODE_H
