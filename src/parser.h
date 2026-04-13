#ifndef NCC_PARSER_H
#define NCC_PARSER_H
#include <functional>

#include "ast/ast_node.h"
#include "lexer.h"
#include "type_scope.h"
#include "ast/nodes/definition_node.h"
#include "ast/nodes/expression_node.h"
#include "ast/nodes/module_node.h"
#include "ast/nodes/struct_node.h"


class TypeNode;

class Parser {
public:
    Parser(std::vector<Lexer> lexer);

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
        std::function<std::unique_ptr<ExpressionNode>(Parser& parser, bool canAssign)> prefix;
        std::function<std::unique_ptr<ExpressionNode>(Parser& parser, std::unique_ptr<ExpressionNode> left,
                                                      bool canAssign)> infix;
        Precedence precedence;
    };

    // Expressions

    std::unordered_map<TokenType, Parser::ParseRule> BuildRules();

    ParseRule Rule(const TokenType& type);

    std::unique_ptr<ExpressionNode> ParsePrecedence(Precedence precedence);

    std::unique_ptr<ExpressionNode> Expression(Precedence start = Precedence::ASSIGNMENT);

    std::unique_ptr<TypeNode> NodeFromType(const Token& token);

    std::unique_ptr<TypeNode> BuildType(std::unique_ptr<TypeNode> base, TypeScope* scope);

    // Statements

    std::unique_ptr<StructNode> StructDeclaration();

    std::unique_ptr<StatementNode> Statement();

    std::unique_ptr<StatementNode> WhileStatement();

    std::unique_ptr<StatementNode> DoWhileStatement();

    std::unique_ptr<StatementNode> ForStatement();

    std::unique_ptr<StatementNode> IfStatement();

    std::unique_ptr<StatementNode> ReturnStatement();

    std::unique_ptr<ExpressionNode> ExpressionStatement();

    ModuleNode* ModuleStatement();

    std::unique_ptr<DefinitionNode> Declaration();

    std::unique_ptr<StatementNode> BlockStatement();

    std::unique_ptr<DefinitionNode> DeclarationStatement();

    void Error(const Token& token, const std::string& message);

    Token Advance();

    Token Peek();

    [[nodiscard]] bool Check(TokenType type) const;

    [[nodiscard]] bool CheckType() const;

    bool Match(TokenType type);

    bool MatchType();

    void Consume(TokenType type, const std::string& error);

    ModuleNode* GetModule(std::string name);

    std::vector<Lexer> lexers_;
    Lexer* lexer_;

    Token current_;
    Token previous_;

    std::unordered_map<std::string, std::unique_ptr<ModuleNode> > modulesList_;
    ModuleNode* module_;
    
    std::unique_ptr<TypeScope> root;
    TypeScope* currentScope;

    static std::unordered_map<TokenType, Parser::ParseRule> rules_;

    friend class Expressions;
};


#endif //NCC_PARSER_H
