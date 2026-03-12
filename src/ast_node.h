#ifndef NCC_AST_NODE_H
#define NCC_AST_NODE_H
#include <memory>
#include <optional>
#include <vector>

#include "lexer.h"

enum AstNodeType {
    //types
    AST_NODE_TYPE_NULL,
    AST_NODE_TYPE_INFER, // for let statements
    AST_NODE_TYPE_VOID,
    AST_NODE_TYPE_BORROW,
    AST_NODE_TYPE_OWNER,
    AST_NODE_TYPE_OPTIONAL,
    AST_NODE_TYPE_ARRAY,
    AST_NODE_TYPE_MAP,
    AST_NODE_TYPE_SIMD,

    AST_NODE_TYPE_BOOL,

    AST_NODE_TYPE_I8,
    AST_NODE_TYPE_I16,
    AST_NODE_TYPE_I32,
    AST_NODE_TYPE_I64,

    AST_NODE_TYPE_U8,
    AST_NODE_TYPE_U16,
    AST_NODE_TYPE_U32,
    AST_NODE_TYPE_U64,

    AST_NODE_TYPE_F32,
    AST_NODE_TYPE_F64,

    AST_NODE_TYPE_TYPE_COUNT, // used for implicit cast table

    AST_NODE_TYPE_TREE,
    AST_NODE_TYPE_SCOPE,
    AST_NODE_TYPE_SEQUENCE,

    //statements
    AST_NODE_TYPE_MODULE_STATEMENT,
    AST_NODE_TYPE_RETURN_STATEMENT,

    //constants
    AST_NODE_TYPE_CHARACTER,
    AST_NODE_TYPE_INTEGER,
    AST_NODE_TYPE_BOOLEAN,
    AST_NODE_TYPE_FLOAT,
    AST_NODE_TYPE_LIST,
    AST_NODE_TYPE_DEFAULT,

    //names
    AST_NODE_TYPE_MODULE_NAME,
    AST_NODE_TYPE_NAME,
    AST_NODE_TYPE_TYPE,

    //declarations OR implementations, depending on context
    AST_NODE_TYPE_VARIABLE, // static
    AST_NODE_TYPE_FUNCTION, // static
    AST_NODE_TYPE_ASSOCIATED, //static abstract methods
    AST_NODE_TYPE_FIELD, // field
    AST_NODE_TYPE_METHOD, // method
    AST_NODE_TYPE_ABSTRACT, //abstract method
    AST_NODE_TYPE_STRUCT,
    AST_NODE_TYPE_INTERFACE,

    AST_NODE_TYPE_IMPLEMENTATION, // implements a function

    //operators
    AST_NODE_TYPE_ASSIGN,
    AST_NODE_TYPE_NEGATE,
    AST_NODE_TYPE_ADDRESS,
    AST_NODE_TYPE_LOCK,
    AST_NODE_TYPE_HEAP,
    AST_NODE_TYPE_NOT,
    AST_NODE_TYPE_ADD,
    AST_NODE_TYPE_SUBTRACT,
    AST_NODE_TYPE_MULTIPLY,
    AST_NODE_TYPE_DIVIDE,
    AST_NODE_TYPE_MODULO,
    AST_NODE_TYPE_POWER,
    AST_NODE_TYPE_BITWISE_AND,
    AST_NODE_TYPE_BITWISE_OR,
    AST_NODE_TYPE_BITWISE_XOR,
    AST_NODE_TYPE_BITWISE_NOT,
    AST_NODE_TYPE_BITWISE_LEFT,
    AST_NODE_TYPE_BITWISE_RIGHT,

    AST_NODE_TYPE_CALL,

    AST_NODE_TYPE_GET,
    AST_NODE_TYPE_INDEX,

    AST_NODE_TYPE_EQUAL,
    AST_NODE_TYPE_NOT_EQUAL,
    AST_NODE_TYPE_GREATER_THAN,
    AST_NODE_TYPE_GREATER_THAN_EQUAL,
    AST_NODE_TYPE_LESS_THAN,
    AST_NODE_TYPE_LESS_THAN_EQUAL,

    AST_NODE_TYPE_AND,
    AST_NODE_TYPE_OR,

    AST_NODE_TYPE_STATIC_CAST,
    AST_NODE_TYPE_REINTERPRET_CAST,

    //control flow
    AST_NODE_TYPE_IF,
    AST_NODE_TYPE_TERNARY,
    AST_NODE_TYPE_WHILE,
    AST_NODE_TYPE_DO_WHILE,

    //complex
    AST_NODE_TYPE_CONTAINS,
};

class AstNode {
public:
    static std::unique_ptr<AstNode> Create(AstNodeType type, std::optional<Token> token);

    AstNode(const AstNode&) = delete;
    AstNode& operator=(const AstNode&) = delete;
    AstNode(AstNode&&) = default;
    AstNode& operator=(AstNode&&) = default;
    ~AstNode();

    [[nodiscard]] AstNode* parent() const;
    AstNode* operator[](size_t index) const;
    void AddNode(std::unique_ptr<AstNode> node);
    void RemoveNode(const AstNode* node);

    AstNodeType type;
    std::optional<Token> token;
private:
    AstNode(AstNodeType type, std::optional<Token> token);

    AstNode* parent_;
    std::vector<std::unique_ptr<AstNode>> children_;
};


#endif //NCC_AST_NODE_H