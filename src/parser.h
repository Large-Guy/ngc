#ifndef NCC_PARSER_H
#define NCC_PARSER_H
#include <functional>

#include "ast_node.h"
#include "lexer.h"


class Parser {
public:
    Parser(Lexer lexer);

    std::vector<std::unique_ptr<AstNode> > Parse();

private:

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
        std::function<std::unique_ptr<AstNode>(Parser& parser, bool canAssign)> prefix;
        std::function<std::unique_ptr<AstNode>(Parser& parser, std::unique_ptr<AstNode> left, bool canAssign)> infix;
        Precedence precedence;
    };

    // Expressions

    std::unordered_map<TokenType, Parser::ParseRule> BuildRules();

    ParseRule Rule(const TokenType& type);

    std::unique_ptr<AstNode> ParsePrecedence(Precedence precedence);

    std::unique_ptr<AstNode> Expression(Precedence start = Precedence::ASSIGNMENT);

    std::unique_ptr<AstNode> NodeFromType(const Token& token);

    std::unique_ptr<AstNode> BuildType(std::unique_ptr<AstNode> base);

    // Statements

    std::unique_ptr<AstNode> Statement();

    std::unique_ptr<AstNode> IfStatement();

    std::unique_ptr<AstNode> ReturnStatement();

    std::unique_ptr<AstNode> ExpressionStatement();

    std::unique_ptr<AstNode> ModuleStatement();

    std::unique_ptr<AstNode> Declaration();

    std::unique_ptr<AstNode> BlockStatement();

    std::unique_ptr<AstNode> DeclarationStatement();

    void Error(const Token& token, const std::string& message);

    Token Advance();

    Token Peek();

    [[nodiscard]] bool Check(TokenType type) const;

    [[nodiscard]] bool CheckType() const;

    bool Match(TokenType type);

    bool MatchType();

    void Consume(TokenType type, const std::string& error);

    Lexer lexer_;

    Token current_;
    Token previous_;

    static std::unordered_map<TokenType, Parser::ParseRule> rules_;

    friend class Expressions;
};


#endif //NCC_PARSER_H
