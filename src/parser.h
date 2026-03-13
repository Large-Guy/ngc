#ifndef NCC_PARSER_H
#define NCC_PARSER_H
#include <functional>

#include "ast_node.h"
#include "lexer.h"


class Parser {
public:
    Parser(Lexer lexer);

    enum class Precedence {
        NONE,
        ASSIGNMENT,
        OR,
        AND,
        TERNARY,
        BITWISE_OR,
        BITWISE_XOR,
        BITWISE_AND,
        EQUALITY,
        COMPARISON,
        SHIFT,
        TERM,
        FACTOR,
        EXPONENT,
        MODULO,
        UNARY,
        CALL,
        PRIMARY
    };

    struct ParseRule {
        std::function<std::unique_ptr<AstNode>(bool canAssign)> prefix;
        std::function<std::unique_ptr<AstNode>(std::unique_ptr<AstNode> left, bool canAssign)> infix;
        Precedence precedence;
    };

    std::unordered_map<TokenType, Parser::ParseRule> BuildRules();

    std::unique_ptr<AstNode> Number(bool canAssign);

    std::unique_ptr<AstNode> String(bool canAssign);

    std::unique_ptr<AstNode> Group(bool canAssign);

    std::unique_ptr<AstNode> Unary(bool canAssign);

    std::unique_ptr<AstNode> Cast(bool canAssign);

    std::unique_ptr<AstNode> Heap(bool canAssign);

    std::unique_ptr<AstNode> Variable(bool canAssign);

    std::unique_ptr<AstNode> Literal(bool canAssign);

    std::unique_ptr<AstNode> Binary(std::unique_ptr<AstNode> left, bool canAssign);

    std::unique_ptr<AstNode> Interval(std::unique_ptr<AstNode> left, bool canAssign);

    std::unique_ptr<AstNode> Include(std::unique_ptr<AstNode> left, bool canAssign);

    std::unique_ptr<AstNode> And(std::unique_ptr<AstNode> left, bool canAssign);

    std::unique_ptr<AstNode> Or(std::unique_ptr<AstNode> left, bool canAssign);

    std::unique_ptr<AstNode> Call(std::unique_ptr<AstNode> left, bool canAssign);

    std::unique_ptr<AstNode> Index(std::unique_ptr<AstNode> left, bool canAssign);

    std::unique_ptr<AstNode> Field(std::unique_ptr<AstNode> left, bool canAssign);

    std::unique_ptr<AstNode> Ternary(std::unique_ptr<AstNode> left, bool canAssign);

    std::vector<std::unique_ptr<AstNode> > Parse();

    std::unique_ptr<AstNode> Expression();

    std::unique_ptr<AstNode> Statement();

    std::unique_ptr<AstNode> ModuleStatement();

    std::unique_ptr<AstNode> BuildType(std::unique_ptr<AstNode> base);

    std::unique_ptr<AstNode> NodeFromType(const Token& token);

    std::unique_ptr<AstNode> Declaration();

    std::unique_ptr<AstNode> Block();

    std::unique_ptr<AstNode> DeclarationStatement();

    void Error(const Token& token, const std::string& message);

    Token Advance();

    Token Peek();

    [[nodiscard]] bool Check(TokenType type) const;

    [[nodiscard]] bool CheckType() const;

    bool MatchType();

    bool Match(TokenType type);

    void Consume(TokenType type, const std::string& error);

    Lexer lexer;

    Token current;
    Token previous;
private:
};


#endif //NCC_PARSER_H
