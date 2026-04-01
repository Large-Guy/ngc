#include "parser.h"
#include "ast/ast_node.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <unordered_set>

#include "ast/nodes/assign_node.h"
#include "ast/nodes/binary_node.h"
#include "ast/nodes/cast_node.h"
#include "ast/nodes/float_node.h"
#include "ast/nodes/heap_node.h"
#include "ast/nodes/identifier_node.h"
#include "ast/nodes/integer_node.h"
#include "ast/nodes/lambda_node.h"
#include "ast/nodes/literal_node.h"
#include "ast/nodes/tuple_node.h"
#include "ast/nodes/type_node.h"
#include "ast/nodes/unary_node.h"
#include "ast/nodes/variable_node.h"
#include "memory_utils.h"
#include "ast/nodes/AddressNode.h"
#include "ast/nodes/call_node.h"
#include "ast/nodes/compound_statement.h"
#include "ast/nodes/do_while_node.h"
#include "ast/nodes/field_node.h"
#include "ast/nodes/for_node.h"
#include "ast/nodes/function_node.h"
#include "ast/nodes/if_node.h"
#include "ast/nodes/index_node.h"
#include "ast/nodes/LockNode.h"
#include "ast/nodes/module_node.h"
#include "ast/nodes/return_node.h"
#include "ast/nodes/while_node.h"

Parser::Parser(std::vector<Lexer> lexer) : lexers_(std::move(lexer)) {
    if (rules_.empty())
        rules_ = BuildRules();
    module_ = nullptr;
}

std::unordered_map<TokenType, Parser::ParseRule> Parser::rules_ = {};

class Expressions {
public:
    // Special expression statement
    static std::unique_ptr<ExpressionNode> Assign(Parser& parser, std::unique_ptr<ExpressionNode> target,
                                                  bool canAssign) {
        auto assignment = parser.previous_;
        std::unique_ptr<ExpressionNode> value;
        switch (assignment.type) {
            case TokenType::EQUAL: {
                value = parser.Expression();
                break;
            }
            case TokenType::PLUS_EQUAL: {
                value = std::make_unique<BinaryNode>(BinaryNodeType::ADD,
                                                     UniqueCast<ExpressionNode>(target->Clone()),
                                                     parser.Expression());
                break;
            }
            case TokenType::MINUS_EQUAL: {
                value = std::make_unique<BinaryNode>(BinaryNodeType::SUBTRACT,
                                                     UniqueCast<ExpressionNode>(target->Clone()),
                                                     parser.Expression());
                break;
            }
            case TokenType::STAR_EQUAL: {
                value = std::make_unique<BinaryNode>(BinaryNodeType::MULTIPLY,
                                                     UniqueCast<ExpressionNode>(target->Clone()),
                                                     parser.Expression());
                break;
            }
            case TokenType::SLASH_EQUAL: {
                value = std::make_unique<BinaryNode>(BinaryNodeType::DIVIDE,
                                                     UniqueCast<ExpressionNode>(target->Clone()),
                                                     parser.Expression());
                break;
            }
            default: {
                parser.Error(assignment, "Invalid assignment operator");
                return nullptr;
            }
        }

        auto assign = std::make_unique<AssignNode>(std::move(target), std::move(value));
        return assign;
    }

    static std::unique_ptr<ExpressionNode> Number(Parser& parser, bool canAssign) {
        auto num = parser.previous_;
        if (parser.previous_.type == TokenType::DOUBLE) {
            return std::make_unique<FloatNode>(std::stod(num.value), true);
        }
        if (parser.previous_.type == TokenType::FLOAT) {
            return std::make_unique<FloatNode>(std::stod(num.value), false);
        }
        return std::make_unique<IntegerNode>(std::stol(num.value));
    }

    static std::unique_ptr<ExpressionNode> String(Parser& parser, bool canAssign) {
        return nullptr;
    }

    static std::unique_ptr<ExpressionNode> Group(Parser& parser, bool canAssign) {
        auto node = parser.Expression();
        if (parser.Match(TokenType::COMMA)) {
            std::vector<std::unique_ptr<ExpressionNode> > list;
            list.push_back(std::move(node));
            do {
                auto next = parser.Expression();
                list.push_back(std::move(next));
            }
            while (parser.Match(TokenType::COMMA));

            return std::make_unique<TupleNode>(std::move(list));
        }

        parser.Consume(TokenType::RIGHT_PAREN, "Expected closing ')'");

        return node;
    }

    static std::unique_ptr<ExpressionNode> Unary(Parser& parser, bool canAssign) {
        auto unary = parser.previous_;
        auto operand = parser.ParsePrecedence(Parser::Precedence::UNARY);
        switch (unary.type) {
            case TokenType::MINUS: {
                auto negate = std::make_unique<UnaryNode>(UnaryNodeType::NEGATE, std::move(operand));
                return negate;
            }
            case TokenType::AND: {
                auto address = std::make_unique<AddressNode>(std::move(operand));
                return address;
            }
            case TokenType::STAR: {
                auto lock = std::make_unique<LockNode>(std::move(operand));
                return lock;
            }
            case TokenType::BANG: {
                auto negate = std::make_unique<UnaryNode>(UnaryNodeType::NOT, std::move(operand));
                return negate;
            }

            default: {
                parser.Error(unary, "Unsupported unary expression type");
                return nullptr;
            }
        }
    }

    static std::unique_ptr<ExpressionNode> Lambda(Parser& parser, bool canAssign) {
        auto return_type = parser.BuildType(parser.NodeFromType(parser.previous_));
        parser.Consume(TokenType::LEFT_PAREN, "Expected '('");

        std::vector<std::unique_ptr<DefinitionNode> > parameters;

        if (!parser.Check(TokenType::RIGHT_PAREN)) {
            do {
                if (!parser.MatchType())
                    parser.Error(parser.current_, "Expected type");
                auto argument = parser.Declaration();
                parameters.push_back(std::move(argument));
            }
            while (parser.Check(TokenType::COMMA));
        }

        parser.Consume(TokenType::RIGHT_PAREN, "Expected ')'");

        auto body = parser.Statement();

        auto function = std::make_unique<LambdaNode>(std::move(return_type), std::move(parameters), std::move(body));
        return function;
    }

    static std::unique_ptr<ExpressionNode> Cast(Parser& parser, std::unique_ptr<ExpressionNode> left, bool canAssign) {
        bool reinterpret = parser.Match(TokenType::BANG);
        parser.Advance();

        auto type = parser.BuildType(parser.NodeFromType(parser.previous_));

        if (reinterpret) {
            return std::make_unique<CastNode>(CastNodeType::REINTERPRET, std::move(type), std::move(left));
        }
        return std::make_unique<CastNode>(CastNodeType::STATIC, std::move(type), std::move(left));
    }

    static std::unique_ptr<ExpressionNode> Heap(Parser& parser, bool canAssign) {
        auto value = parser.Expression();
        parser.Consume(TokenType::RIGHT_BRACKET, "Expected closing ']'");
        return std::make_unique<HeapNode>(std::move(value));
    }

    static std::unique_ptr<ExpressionNode> Variable(Parser& parser, bool canAssign) {
        auto variable = parser.previous_;
        return std::make_unique<IdentifierNode>(variable.value);
    }

    static std::unique_ptr<ExpressionNode> Literal(Parser& parser, bool canAssign) {
        auto token = parser.previous_;
        switch (token.type) {
            case TokenType::NULL_:
                return std::make_unique<LiteralNode>(LiteralNodeType::_NULL);
            case TokenType::TRUE:
                return std::make_unique<LiteralNode>(LiteralNodeType::TRUE);
            case TokenType::FALSE:
                return std::make_unique<LiteralNode>(LiteralNodeType::FALSE);
            default: {
                parser.Error(token, "Unsupported literal type");
                return nullptr;
            }
        }
    }

    static std::unique_ptr<ExpressionNode>
    Binary(Parser& parser, std::unique_ptr<ExpressionNode> left, bool canAssign) {
        auto op = parser.previous_;

        auto rule = parser.Rule(op.type);

        auto right = parser.ParsePrecedence(static_cast<Parser::Precedence>(static_cast<int>(rule.precedence) + 1));

        std::unique_ptr<ExpressionNode> node = nullptr;

        switch (op.type) {
            case TokenType::PLUS: {
                return std::make_unique<BinaryNode>(BinaryNodeType::ADD, std::move(left), std::move(right));
            }
            case TokenType::MINUS: {
                return std::make_unique<BinaryNode>(BinaryNodeType::SUBTRACT, std::move(left), std::move(right));
            }
            case TokenType::STAR: {
                return std::make_unique<BinaryNode>(BinaryNodeType::MULTIPLY, std::move(left), std::move(right));
            }
            case TokenType::SLASH: {
                return std::make_unique<BinaryNode>(BinaryNodeType::DIVIDE, std::move(left), std::move(right));
            }
            case TokenType::STAR_STAR: {
                return std::make_unique<BinaryNode>(BinaryNodeType::EXPONENT, std::move(left), std::move(right));
            }
            case TokenType::PERCENT: {
                return std::make_unique<BinaryNode>(BinaryNodeType::MODULO, std::move(left), std::move(right));
            }
            case TokenType::PIPE: {
                return std::make_unique<BinaryNode>(BinaryNodeType::BITWISE_OR, std::move(left), std::move(right));
            }
            case TokenType::CARET: {
                return std::make_unique<BinaryNode>(BinaryNodeType::BITWISE_XOR, std::move(left), std::move(right));
            }
            case TokenType::AND: {
                return std::make_unique<BinaryNode>(BinaryNodeType::BITWISE_AND, std::move(left), std::move(right));
            }
            case TokenType::GREATER: {
                return std::make_unique<BinaryNode>(BinaryNodeType::GREATER, std::move(left), std::move(right));
            }
            case TokenType::GREATER_EQUAL: {
                return std::make_unique<BinaryNode>(BinaryNodeType::GREATER_EQUAL, std::move(left), std::move(right));
            }
            case TokenType::LESS: {
                return std::make_unique<BinaryNode>(BinaryNodeType::LESS, std::move(left), std::move(right));
            }
            case TokenType::LESS_EQUAL: {
                return std::make_unique<BinaryNode>(BinaryNodeType::LESS_EQUAL, std::move(left), std::move(right));
            }
            case TokenType::GREATER_GREATER: {
                return std::make_unique<BinaryNode>(BinaryNodeType::BITWISE_RIGHT, std::move(left), std::move(right));
            }
            case TokenType::EQUAL_EQUAL: {
                return std::make_unique<BinaryNode>(BinaryNodeType::EQUAL, std::move(left), std::move(right));
            }
            case TokenType::LESS_LESS: {
                return std::make_unique<BinaryNode>(BinaryNodeType::BITWISE_LEFT, std::move(left), std::move(right));
            }
            case TokenType::AND_AND: {
                return std::make_unique<BinaryNode>(BinaryNodeType::AND, std::move(left), std::move(right));
            }
            case TokenType::PIPE_PIPE: {
                return std::make_unique<BinaryNode>(BinaryNodeType::OR, std::move(left), std::move(right));
            }
            default: {
                parser.Error(op, "Unsupported binary expression type");
                return nullptr;
            }
        }

        return node;
    }

    static std::unique_ptr<ExpressionNode> Interval(Parser& parser, std::unique_ptr<ExpressionNode> left,
                                                    bool canAssign) {
        auto group = [&](std::unique_ptr<ExpressionNode> prior) -> std::unique_ptr<ExpressionNode> {
            auto enter = parser.current_;
            parser.Advance();

            auto a = parser.Expression();
            parser.Consume(TokenType::COMMA, "Expected ','");
            auto b = parser.Expression();

            std::unique_ptr<BinaryNode> entry = nullptr;
            if (enter.type == TokenType::LEFT_BRACKET) {
                entry = std::make_unique<BinaryNode>(BinaryNodeType::GREATER_EQUAL,
                                                     UniqueCast<ExpressionNode>(prior->Clone()), std::move(a));
            } else if (enter.type == TokenType::LEFT_PAREN) {
                entry = std::make_unique<BinaryNode>(BinaryNodeType::GREATER,
                                                     UniqueCast<ExpressionNode>(prior->Clone()), std::move(a));
            } else {
                parser.Error(parser.previous_, "Expected '(' or '['");
                return nullptr;
            }
            std::shared_ptr<BinaryNode> node;
            std::unique_ptr<BinaryNode> exit = nullptr;
            if (parser.Match(TokenType::RIGHT_BRACKET)) {
                exit = std::make_unique<BinaryNode>(BinaryNodeType::LESS_EQUAL,
                                                    UniqueCast<ExpressionNode>(prior->Clone()), std::move(b));
            } else if (parser.Match(TokenType::RIGHT_PAREN)) {
                exit = std::make_unique<BinaryNode>(BinaryNodeType::LESS, UniqueCast<ExpressionNode>(prior->Clone()),
                                                    std::move(b));
            } else {
                parser.Error(parser.previous_, "Expected ')' or ']'");
                return nullptr;
            }
            auto and_ = std::make_unique<BinaryNode>(BinaryNodeType::AND, std::move(entry), std::move(exit));
            return and_;
        };
        auto root = group(UniqueCast<ExpressionNode>(left->Clone()));
        while (!parser.Match(TokenType::RIGHT_BRACE)) {
            std::unique_ptr<AstNode> node = nullptr;
            if (parser.Match(TokenType::COMMA)) {
                node = std::make_unique<BinaryNode>(BinaryNodeType::EQUAL, UniqueCast<ExpressionNode>(left->Clone()),
                                                    UniqueCast<ExpressionNode>(parser.Expression()));
            } else if (parser.Match(TokenType::IDENTIFIER) &&
                       (parser.previous_.value == "u" || parser.previous_.value == "U")
            ) {
                node = group(UniqueCast<ExpressionNode>(left->Clone()));
            } else {
                parser.Error(parser.previous_, "Expected ',', 'u' or 'U'");
            }
            auto or_ = std::make_unique<BinaryNode>(BinaryNodeType::OR, UniqueCast<ExpressionNode>(std::move(root)),
                                                    UniqueCast<ExpressionNode>(std::move(node)));
            root = std::move(or_);
        }
        return root;
    }

    static std::unique_ptr<ExpressionNode>
    Include(Parser& parser, std::unique_ptr<ExpressionNode> left, bool canAssign) {
        /*auto contains = AstNode::New(AstNodeType::CONTAINS);
        contains->AddNode(std::move(left));
        auto container = parser.Expression();
        contains->AddNode(std::move(container));
        return contains;*/
        throw std::runtime_error("Include not implemented");
    }

    static std::unique_ptr<ExpressionNode> Call(Parser& parser, std::unique_ptr<ExpressionNode> left, bool canAssign) {
        std::vector<std::unique_ptr<ExpressionNode> > args;
        if (!parser.Check(TokenType::RIGHT_PAREN)) {
            do {
                auto argument = parser.Expression();
                args.push_back(std::move(argument));
            }
            while (parser.Match(TokenType::COMMA));
        }

        parser.Consume(TokenType::RIGHT_PAREN, "Expected closing ')'");

        return std::make_unique<CallNode>(std::move(left), std::move(args));
    }

    static std::unique_ptr<ExpressionNode> Index(Parser& parser, std::unique_ptr<ExpressionNode> left, bool canAssign) {
        auto index = parser.Expression();
        parser.Consume(TokenType::RIGHT_BRACKET, "Expected closing ']'");

        return std::make_unique<IndexNode>(std::move(left), std::move(index));
    }

    static std::unique_ptr<ExpressionNode> Field(Parser& parser, std::unique_ptr<ExpressionNode> left, bool canAssign) {
        parser.Consume(TokenType::IDENTIFIER, "Expected field name");

        return std::make_unique<FieldNode>(std::move(left), parser.previous_.value);
    }

    static std::unique_ptr<ExpressionNode>
    Ternary(Parser& parser, std::unique_ptr<ExpressionNode> left, bool canAssign) {
        return nullptr;
    }

    static std::unique_ptr<ExpressionNode> Sequence(Parser& parser, std::unique_ptr<ExpressionNode> left,
                                                    bool canAssign) {
        /*auto node = AstNode::New(AstNodeType::SEQUENCE);
        node->AddNode(std::move(left));
        auto sequence = parser.Expression();
        node->AddNode(std::move(sequence));
        return node;*/
        return nullptr;
    }
};

std::unordered_map<TokenType, Parser::ParseRule> Parser::BuildRules() {
    std::unordered_map<TokenType, ParseRule> rules{};
    rules[TokenType::LEFT_PAREN] = {Expressions::Group, Expressions::Call, Precedence::CALL};
    rules[TokenType::LEFT_BRACKET] = {Expressions::Heap, Expressions::Index, Precedence::CALL};
    rules[TokenType::LEFT_BRACE] = {nullptr, Expressions::Interval, Precedence::CALL};
    rules[TokenType::DOT] = {nullptr, Expressions::Field, Precedence::CALL};
    rules[TokenType::DOT_DOT] = {nullptr, Expressions::Sequence, Precedence::CALL};
    rules[TokenType::PLUS] = {nullptr, Expressions::Binary, Precedence::TERM};
    rules[TokenType::PLUS_EQUAL] = {nullptr, Expressions::Assign, Precedence::ASSIGNMENT};
    rules[TokenType::MINUS] = {Expressions::Unary, Expressions::Binary, Precedence::TERM};
    rules[TokenType::MINUS_EQUAL] = {nullptr, Expressions::Assign, Precedence::ASSIGNMENT};
    rules[TokenType::STAR] = {Expressions::Unary, Expressions::Binary, Precedence::FACTOR};
    rules[TokenType::STAR_STAR] = {nullptr, Expressions::Binary, Precedence::EXPONENT};
    rules[TokenType::STAR_EQUAL] = {nullptr, Expressions::Assign, Precedence::ASSIGNMENT};
    rules[TokenType::SLASH] = {nullptr, Expressions::Binary, Precedence::FACTOR};
    rules[TokenType::SLASH_EQUAL] = {nullptr, Expressions::Assign, Precedence::ASSIGNMENT};
    rules[TokenType::BANG] = {Expressions::Unary, nullptr, Precedence::TERM};
    rules[TokenType::BANG_EQUAL] = {nullptr, Expressions::Binary, Precedence::EQUALITY};
    rules[TokenType::EQUAL_EQUAL] = {nullptr, Expressions::Binary, Precedence::EQUALITY};
    rules[TokenType::GREATER] = {nullptr, Expressions::Binary, Precedence::COMPARISON};
    rules[TokenType::GREATER_GREATER] = {nullptr, Expressions::Binary, Precedence::SHIFT};
    rules[TokenType::GREATER_EQUAL] = {nullptr, Expressions::Binary, Precedence::COMPARISON};
    rules[TokenType::LESS] = {nullptr, Expressions::Binary, Precedence::COMPARISON};
    rules[TokenType::LESS_LESS] = {nullptr, Expressions::Binary, Precedence::SHIFT};
    rules[TokenType::LESS_EQUAL] = {nullptr, Expressions::Binary, Precedence::COMPARISON};
    rules[TokenType::AND] = {Expressions::Unary, Expressions::Binary, Precedence::BITWISE_AND};
    rules[TokenType::AND_AND] = {nullptr, Expressions::Binary, Precedence::AND};
    rules[TokenType::PIPE] = {nullptr, Expressions::Binary, Precedence::BITWISE_OR};
    rules[TokenType::PIPE_PIPE] = {nullptr, Expressions::Binary, Precedence::OR};
    rules[TokenType::PERCENT] = {nullptr, Expressions::Binary, Precedence::MODULO};
    rules[TokenType::CARET] = {nullptr, Expressions::Binary, Precedence::BITWISE_XOR};
    rules[TokenType::TILDE] = {Expressions::Unary, nullptr, Precedence::UNARY};
    rules[TokenType::STRING_LITERAL] = {Expressions::String, nullptr, Precedence::NONE};
    rules[TokenType::CHARACTER] = {Expressions::Number, nullptr, Precedence::NONE};
    rules[TokenType::INTEGER] = {Expressions::Number, nullptr, Precedence::NONE};
    rules[TokenType::FLOAT] = {Expressions::Number, nullptr, Precedence::NONE};
    rules[TokenType::DOUBLE] = {Expressions::Number, nullptr, Precedence::NONE};
    rules[TokenType::IDENTIFIER] = {Expressions::Variable, nullptr, Precedence::NONE};
    rules[TokenType::I8] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::I16] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::I32] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::I64] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::U8] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::U16] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::U32] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::U64] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::F32] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::F64] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::ISIZE] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::USIZE] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::BOOL] = {Expressions::Lambda, nullptr, Precedence::UNARY};
    rules[TokenType::AS] = {nullptr, Expressions::Cast, Precedence::UNARY};
    rules[TokenType::NULL_] = {Expressions::Literal, nullptr, Precedence::NONE};
    rules[TokenType::TRUE] = {Expressions::Literal, nullptr, Precedence::NONE};
    rules[TokenType::FALSE] = {Expressions::Literal, nullptr, Precedence::NONE};
    rules[TokenType::IN] = {nullptr, Expressions::Include, Precedence::CALL};
    rules[TokenType::QUESTION] = {nullptr, Expressions::Ternary, Precedence::TERNARY};
    rules[TokenType::EQUAL] = {nullptr, Expressions::Assign, Precedence::ASSIGNMENT};
    return rules;
};

std::vector<std::unique_ptr<AstNode> > Parser::Parse() {
    std::vector<std::unique_ptr<AstNode> > nodes = {};
    for (auto& lexer: lexers_) {
        lexer_ = &lexer;
        Advance();
        while (!Match(TokenType::EOF_)) {
            if (module_ == nullptr) {
                auto statement = Statement();
                if (statement != nullptr) {
                    throw std::runtime_error("Expected module statement");
                }
                continue;
            }
            auto statement = Statement();
            module_->statements.push_back(std::move(statement));
        }
    }

    for (auto& module: modulesList_) {
        nodes.push_back(std::move(module.second));
    }

    return nodes;
}

Parser::ParseRule Parser::Rule(const TokenType& type) {
    if (rules_.find(type) != rules_.end()) {
        return rules_[type];
    }
    return {};
}

std::unique_ptr<ExpressionNode> Parser::ParsePrecedence(Precedence precedence) {
    Advance();
    auto prefix = Rule(previous_.type).prefix;
    if (prefix == nullptr) {
        Error(previous_, "Expected valid prefix");
        return nullptr;
    }

    bool can_assign = precedence <= Precedence::ASSIGNMENT;
    auto result = prefix(*this, can_assign);

    while (precedence <= Rule(current_.type).precedence) {
        Advance();
        auto infix = Rule(previous_.type).infix;
        if (infix == nullptr) {
            Error(previous_, "Expected type");
        }
        result = infix(*this, std::move(result), can_assign);
    }

    return result;
}

std::unique_ptr<ExpressionNode> Parser::Expression(Precedence start) {
    return ParsePrecedence(start);
}

std::unique_ptr<TypeNode> Parser::NodeFromType(const Token& token) {
    switch (token.type) {
        case TokenType::VOID:
            return std::make_unique<TypeNode>(TypeNodeType::VOID);
        case TokenType::I8:
            return std::make_unique<TypeNode>(TypeNodeType::I8);
        case TokenType::I16:
            return std::make_unique<TypeNode>(TypeNodeType::I16);
        case TokenType::I32:
            return std::make_unique<TypeNode>(TypeNodeType::I32);
        case TokenType::I64:
            return std::make_unique<TypeNode>(TypeNodeType::I64);
        case TokenType::U8:
            return std::make_unique<TypeNode>(TypeNodeType::U8);
        case TokenType::U16:
            return std::make_unique<TypeNode>(TypeNodeType::U16);
        case TokenType::U32:
            return std::make_unique<TypeNode>(TypeNodeType::U32);
        case TokenType::U64:
            return std::make_unique<TypeNode>(TypeNodeType::U64);
        case TokenType::F32:
            return std::make_unique<TypeNode>(TypeNodeType::F32);
        case TokenType::F64:
            return std::make_unique<TypeNode>(TypeNodeType::F64);
        case TokenType::BOOL:
            return std::make_unique<TypeNode>(TypeNodeType::BOOL);
        case TokenType::LET:
            return nullptr;
        default: {
            Error(token, "Unsupported type");
            return nullptr;
        }
    }
}

std::unique_ptr<TypeNode> Parser::BuildType(std::unique_ptr<TypeNode> base) {
    if (Match(TokenType::LESS)) {
        auto size = Expression(Precedence::SHIFT);
        Consume(TokenType::GREATER, "Expected closing '>'");
        auto simd = std::make_unique<TypeNode>(TypeNodeType::SIMD, std::move(base), std::move(size));
        return BuildType(std::move(simd));
    }
    if (Match(TokenType::STAR)) {
        auto owner = std::make_unique<TypeNode>(TypeNodeType::OWNER, std::move(base));
        return BuildType(std::move(owner));
    }
    if (Match(TokenType::AND)) {
        auto borrow = std::make_unique<TypeNode>(TypeNodeType::BORROW, std::move(base));
        return BuildType(std::move(borrow));
    }
    if (Match(TokenType::QUESTION)) {
        auto borrow = std::make_unique<TypeNode>(TypeNodeType::OPTIONAL, std::move(base));
        return BuildType(std::move(borrow));
    }
    if (Match(TokenType::LEFT_BRACKET)) {
        Consume(TokenType::RIGHT_BRACKET, "Expected closing ']'");
        auto dynamic_array = std::make_unique<TypeNode>(TypeNodeType::ARRAY, std::move(base));
        return BuildType(std::move(dynamic_array));
    }
    return base;
}

std::unique_ptr<StatementNode> Parser::Statement() {
    if (Match(TokenType::MODULE)) {
        module_ = ModuleStatement();
        return nullptr;
    }
    if (MatchType()) {
        return DeclarationStatement();
    }
    if (Match(TokenType::LEFT_BRACE)) {
        return BlockStatement();
    }
    if (Match(TokenType::IF)) {
        return IfStatement();
    }
    if (Match(TokenType::WHILE)) {
        return WhileStatement();
    }
    if (Match(TokenType::DO)) {
        return DoWhileStatement();
    }
    if (Match(TokenType::FOR)) {
        return ForStatement();
    }
    if (Match(TokenType::RETURN)) {
        return ReturnStatement();
    }
    return ExpressionStatement();
}

std::unique_ptr<StatementNode> Parser::WhileStatement() {
    Consume(TokenType::LEFT_PAREN, "Expected '('");
    auto condition = Expression();
    Consume(TokenType::RIGHT_PAREN, "Expected ')'");
    auto body = Statement();

    return std::make_unique<WhileNode>(std::move(condition), std::move(body));
}

std::unique_ptr<StatementNode> Parser::DoWhileStatement() {
    auto body = Statement();
    Consume(TokenType::WHILE, "Expected 'while'");

    Consume(TokenType::LEFT_PAREN, "Expected '('");
    auto condition = Expression();
    Consume(TokenType::RIGHT_PAREN, "Expected ')'");

    return std::make_unique<DoWhileNode>(std::move(body), std::move(condition));
}

std::unique_ptr<StatementNode> Parser::ForStatement() {
    Consume(TokenType::LEFT_PAREN, "Expected '('");
    auto initializer = Statement();
    auto condition = Expression();
    Consume(TokenType::SEMICOLON, "Expected ';'");
    auto increment = Expression();
    Consume(TokenType::RIGHT_PAREN, "Expected ')'");
    auto body = Statement();

    return std::make_unique<ForNode>(std::move(initializer), std::move(condition), std::move(increment),
                                     std::move(body));
}

std::unique_ptr<StatementNode> Parser::IfStatement() {
    Consume(TokenType::LEFT_PAREN, "Expected '('");
    auto condition = Expression();
    Consume(TokenType::RIGHT_PAREN, "Expected ')'");
    auto then_branch = Statement();

    if (Match(TokenType::ELSE)) {
        auto else_branch = Statement();
        return std::make_unique<IfNode>(std::move(condition), std::move(then_branch), std::move(else_branch));
    }
    return std::make_unique<IfNode>(std::move(condition), std::move(then_branch), nullptr);
}

std::unique_ptr<StatementNode> Parser::ReturnStatement() {
    if (Match(TokenType::SEMICOLON)) {
        return std::make_unique<ReturnNode>(nullptr);
    }
    auto what = Expression();
    Consume(TokenType::SEMICOLON, "Expected semicolon");
    return std::make_unique<ReturnNode>(std::move(what));
}

std::unique_ptr<ExpressionNode> Parser::ExpressionStatement() {
    auto expression = Expression();
    Consume(TokenType::SEMICOLON, "Expected semicolon");
    return expression;
}

ModuleNode* Parser::ModuleStatement() {
    Consume(TokenType::IDENTIFIER, "Expected identifier");
    std::string name = previous_.value;
    ModuleNode* current = nullptr;
    if (!modulesList_.contains(name)) {
        modulesList_[name] = std::make_unique<ModuleNode>(name);
    }
    current = modulesList_[name].get();
    while (Match(TokenType::DOT)) {
        Consume(TokenType::IDENTIFIER, "Expected identifier");
        name = previous_.value;

        if (current->GetSubmodule(name)) {
            current = current->GetSubmodule(name);
        } else {
            auto submodule = std::make_unique<ModuleNode>(name);
            current = submodule.get();
        }
    }

    Consume(TokenType::SEMICOLON, "Expected semicolon after statement");

    return current;
}

std::unique_ptr<DefinitionNode> Parser::Declaration() {
    auto type_token = previous_;
    auto base_node = NodeFromType(type_token);
    auto type_node = BuildType(std::move(base_node));

    Consume(TokenType::IDENTIFIER, "Expected name after definition");
    auto name = previous_.value;

    if (Match(TokenType::LEFT_PAREN)) {
        std::vector<std::unique_ptr<DefinitionNode> > args;

        if (!Check(TokenType::RIGHT_PAREN)) {
            do {
                if (!MatchType())
                    Error(current_, "Expected type");
                args.push_back(std::move(Declaration()));
            }
            while (Match(TokenType::COMMA));
        }

        Consume(TokenType::RIGHT_PAREN, "Expected ')'");

        return std::make_unique<FunctionNode>(name, std::move(type_node), std::move(args), nullptr);
    }

    return std::make_unique<VariableNode>(name, std::move(type_node), nullptr);
}

std::unique_ptr<StatementNode> Parser::BlockStatement() {
    std::vector<std::unique_ptr<StatementNode> > statements;
    while (!Match(TokenType::RIGHT_BRACE)) {
        statements.push_back(Statement());
    }
    return std::make_unique<CompoundStatement>(std::move(statements));
}

std::unique_ptr<DefinitionNode> Parser::DeclarationStatement() {
    auto definition = Declaration();
    if (auto variable = UniqueCast<VariableNode>(definition)) {
        if (Match(TokenType::EQUAL)) {
            variable->value = std::move(Expression());
        }
        Consume(TokenType::SEMICOLON, "Expected ';'");
        return variable;
    }
    if (auto function = UniqueCast<FunctionNode>(definition)) {
        function->body = Statement();
        return function;
    }
    throw std::runtime_error("Unexpected declaration type");
}

void Parser::Error(const Token& token, const std::string& message) {
    std::cerr << message << " at line: " << token.line << " at: " << token.value << std::endl;
    throw std::exception();
}

Token Parser::Advance() {
    previous_ = current_;
    while (true) {
        current_ = lexer_->Next();
        if (current_.type != TokenType::ERROR)
            break;

        Error(current_, "Unexpected error");
        break;
    }
    std::cout << TokenTypeToString(current_.type) << std::endl;
    return current_;
}

Token Parser::Peek() {
    return current_;
}

bool Parser::Check(TokenType type) const {
    return current_.type == type;
}

bool Parser::CheckType() const {
    if (Check(TokenType::VOID)) {
        return true;
    }
    if (Check(TokenType::I8)) {
        return true;
    }
    if (Check(TokenType::I16)) {
        return true;
    }
    if (Check(TokenType::I32)) {
        return true;
    }
    if (Check(TokenType::I64)) {
        return true;
    }
    if (Check(TokenType::U8)) {
        return true;
    }
    if (Check(TokenType::U16)) {
        return true;
    }
    if (Check(TokenType::U32)) {
        return true;
    }
    if (Check(TokenType::U64)) {
        return true;
    }
    if (Check(TokenType::F32)) {
        return true;
    }
    if (Check(TokenType::F64)) {
        return true;
    }
    if (Check(TokenType::BOOL)) {
        return true;
    }
    if (Check(TokenType::STRING)) {
        return true;
    }
    if (Check(TokenType::LET)) {
        return true;
    }
    return false;
}

bool Parser::Match(TokenType type) {
    if (Check(type)) {
        Advance();
        return true;
    }
    return false;
}

bool Parser::MatchType() {
    if (CheckType()) {
        Advance();
        return true;
    }
    return false;
}

void Parser::Consume(TokenType type, const std::string& error) {
    if (Check(type)) {
        Advance();
        return;
    }
    Error(current_, error);
}
