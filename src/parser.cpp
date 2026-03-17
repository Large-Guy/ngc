#include "parser.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <unordered_set>

Parser::Parser(Lexer lexer) : lexer_(std::move(lexer)) {
    if (rules_.empty())
        rules_ = BuildRules();
    Advance();
}

std::unordered_map<TokenType, Parser::ParseRule> Parser::rules_ = {};

class Expressions {
public:
    // Special expression statement
    static std::unique_ptr<AstNode> Assign(Parser& parser, std::unique_ptr<AstNode> target, bool canAssign) {
        auto assignment = parser.previous_;
        switch (assignment.type) {
            case TokenType::EQUAL: {
                auto assign = AstNode::New(AstNodeType::ASSIGN);
                assign->AddNode(std::move(target));
                assign->AddNode(std::move(parser.Expression()));
                return assign;
            }
            default: {
                parser.Error(assignment, "Invalid assignment operator");
                return nullptr;
            }
        }
    }
    
    static std::unique_ptr<AstNode> Number(Parser& parser, bool canAssign) {
        auto num = parser.previous_;
        if (parser.previous_.type == TokenType::FLOAT) {
            return AstNode::New(AstNodeType::FLOAT, num);
        }
        return AstNode::New(AstNodeType::INT, num);
    }

    static std::unique_ptr<AstNode> String(Parser& parser, bool canAssign) {
    }

    static std::unique_ptr<AstNode> Group(Parser& parser, bool canAssign) {
        auto node = parser.Expression();
        if (parser.Match(TokenType::COMMA)) {
            auto list = AstNode::New(AstNodeType::LIST);
            list->AddNode(std::move(node));
            do {
                auto next = parser.Expression();
                list->AddNode(std::move(next));
            } while (parser.Match(TokenType::COMMA));

            node = std::move(list);
        }

        parser.Consume(TokenType::RIGHT_PAREN, "Expected closing ')'");

        return node;
    }

    static std::unique_ptr<AstNode> Unary(Parser& parser, bool canAssign) {
        auto unary = parser.previous_;
        auto operand = parser.ParsePrecedence(Parser::Precedence::UNARY);
        switch (unary.type) {
            case TokenType::MINUS: {
                auto negate = AstNode::New(AstNodeType::NEGATE);
                negate->AddNode(std::move(operand));
                return negate;
            }
            case TokenType::AND: {
                auto address = AstNode::New(AstNodeType::ADDRESS);
                address->AddNode(std::move(operand));
                return address;
            }
            case TokenType::STAR: {
                auto negate = AstNode::New(AstNodeType::LOCK);
                negate->AddNode(std::move(operand));
                return negate;
            }
            case TokenType::BANG: {
                auto negate = AstNode::New(AstNodeType::NOT);
                negate->AddNode(std::move(operand));
                return negate;
            }
                
            default: {
                parser.Error(unary, "Unsupported unary expression type");
                return nullptr;
            }
        }
    }

    static std::unique_ptr<AstNode> Lambda(Parser& parser, bool canAssign) {
        auto type = parser.BuildType(parser.NodeFromType(parser.previous_));
        parser.Consume(TokenType::LEFT_PAREN, "Expected '('");
        //lambda
        auto function = AstNode::New(AstNodeType::LAMBDA);
        function->AddNode(std::move(type));

        if (!parser.Check(TokenType::RIGHT_PAREN)) {
            do {
                if (!parser.MatchType())
                    parser.Error(parser.current_, "Expected type");
                auto argument = parser.Declaration();
                function->AddNode(std::move(argument));
            } while (parser.Check(TokenType::COMMA));
        }

        parser.Consume(TokenType::RIGHT_PAREN, "Expected ')'");

        auto body = parser.Statement();
        function->AddNode(std::move(body));

        return function;
    }

    static std::unique_ptr<AstNode> Cast(Parser& parser, std::unique_ptr<AstNode> left, bool canAssign) {
        std::unique_ptr<AstNode> cast;
        if (parser.Match(TokenType::BANG)) {
            cast = AstNode::New(AstNodeType::REINTERPRET);
        }
        else {
            cast = AstNode::New(AstNodeType::CAST);
        }
        parser.Advance();
        auto type = parser.BuildType(parser.NodeFromType(parser.previous_));
        cast->AddNode(std::move(type));
        cast->AddNode(std::move(left));

        return cast;
    }

    static std::unique_ptr<AstNode> Heap(Parser& parser, bool canAssign) {
        auto node = AstNode::New(AstNodeType::HEAP);
        auto value = parser.Expression();
        parser.Consume(TokenType::RIGHT_BRACKET, "Expected closing ']'");
        node->AddNode(std::move(value));
        return node;
    }

    static std::unique_ptr<AstNode> Variable(Parser& parser, bool canAssign) {
        auto variable = parser.previous_;
        auto variable_name = AstNode::New(AstNodeType::NAME, variable);
        return variable_name;
    }

    static std::unique_ptr<AstNode> Literal(Parser& parser, bool canAssign) {
        auto token = parser.previous_;
        switch (token.type) {
            case TokenType::NULL_:
                return AstNode::New(AstNodeType::_NULL);
            case TokenType::TRUE:
                return AstNode::New(AstNodeType::TRUE);
            case TokenType::FALSE:
                return AstNode::New(AstNodeType::FALSE);
            default: {
                parser.Error(token, "Unsupported literal type");
                return nullptr;
            }
        }
    }

    static std::unique_ptr<AstNode> Binary(Parser& parser, std::unique_ptr<AstNode> left, bool canAssign) {
        auto op = parser.previous_;

        auto rule = parser.Rule(op.type);

        auto right = parser.ParsePrecedence(static_cast<Parser::Precedence>(static_cast<int>(rule.precedence) + 1));

        std::unique_ptr<AstNode> node = nullptr;

        switch (op.type) {
            case TokenType::PLUS: {
                node = AstNode::New(AstNodeType::ADD);
                break;
            }
            case TokenType::MINUS: {
                node = AstNode::New(AstNodeType::SUBTRACT);
                break;
            }
            case TokenType::STAR: {
                node = AstNode::New(AstNodeType::MULTIPLY);
                break;
            }
            case TokenType::SLASH: {
                node = AstNode::New(AstNodeType::DIVIDE);
                break;
            }
            case TokenType::STAR_STAR: {
                node = AstNode::New(AstNodeType::EXPONENT);
                break;
            }
            case TokenType::PERCENT: {
                node = AstNode::New(AstNodeType::MODULO);
                break;
            }
            case TokenType::PIPE: {
                node = AstNode::New(AstNodeType::BITWISE_OR);
                break;
            }
            case TokenType::CARET: {
                node = AstNode::New(AstNodeType::BITWISE_XOR);
                break;
            }
            case TokenType::AND: {
                node = AstNode::New(AstNodeType::BITWISE_AND);
                break;
            }
            case TokenType::GREATER: {
                node = AstNode::New(AstNodeType::GREATER);
                break;
            }
            case TokenType::GREATER_EQUAL: {
                node = AstNode::New(AstNodeType::GREATER_EQUAL);
                break;
            }
            case TokenType::LESS: {
                node = AstNode::New(AstNodeType::LESS);
                break;
            }
            case TokenType::LESS_EQUAL: {
                node = AstNode::New(AstNodeType::LESS_EQUAL);
                break;
            }
            case TokenType::GREATER_GREATER: {
                node = AstNode::New(AstNodeType::SHIFT_RIGHT);
                break;
            }
            case TokenType::EQUAL_EQUAL: {
                node = AstNode::New(AstNodeType::EQUAL);
                break;
            }
            case TokenType::LESS_LESS: {
                node = AstNode::New(AstNodeType::SHIFT_LEFT);
                break;
            }
            case TokenType::AND_AND: {
                node = AstNode::New(AstNodeType::AND);
                break;
            }
            case TokenType::PIPE_PIPE: {
                node = AstNode::New(AstNodeType::OR);
                break;
            }
            default: {
                parser.Error(op, "Unsupported binary expression type");
                return nullptr;
            }
        }

        node->AddNode(std::move(left));
        node->AddNode(std::move(right));

        return node;
    }

    static std::unique_ptr<AstNode> Interval(Parser& parser, std::unique_ptr<AstNode> left, bool canAssign) {
        auto group = [&](std::unique_ptr<AstNode> prior) -> std::unique_ptr<AstNode> {
            std::unique_ptr<AstNode> entry = nullptr;
            if (parser.Match(TokenType::LEFT_BRACKET)) {
                entry = AstNode::New(AstNodeType::GREATER_EQUAL);
            } else if (parser.Match(TokenType::LEFT_PAREN)) {
                entry = AstNode::New(AstNodeType::GREATER);
            }
            else {
                parser.Error(parser.previous_, "Expected '(' or '['");
                return nullptr;
            }
            auto a = parser.Expression();
            parser.Consume(TokenType::COMMA, "Expected ','");
            auto b = parser.Expression();
            std::unique_ptr<AstNode> exit = nullptr;
            if (parser.Match(TokenType::RIGHT_BRACKET)) {
                exit = AstNode::New(AstNodeType::LESS_EQUAL);
            } else if (parser.Match(TokenType::RIGHT_PAREN)) {
                exit = AstNode::New(AstNodeType::LESS);
            }
            else {
                parser.Error(parser.previous_, "Expected ')' or ']'");
                return nullptr;
            }
            auto and_ = AstNode::New(AstNodeType::AND);
            entry->AddNode(std::move(prior->Clone()));
            entry->AddNode(std::move(a));

            exit->AddNode(std::move(prior->Clone()));
            exit->AddNode(std::move(b));

            and_->AddNode(std::move(entry));
            and_->AddNode(std::move(exit));
            return and_;
        };
        std::unique_ptr<AstNode> root = group(std::move(left->Clone()));
        while (!parser.Match(TokenType::RIGHT_BRACE)) {
            std::unique_ptr<AstNode> node = nullptr;
            if (parser.Match(TokenType::COMMA)) {

                node = AstNode::New(AstNodeType::EQUAL);
                node->AddNode(std::move(left->Clone()));
                node->AddNode(std::move(parser.Expression()));

            }
            else if (parser.Match(TokenType::IDENTIFIER) &&
                (parser.previous_.value == "u" || parser.previous_.value == "U")
                ) {
                node = group(std::move(left->Clone()));
            }
            else {
                parser.Error(parser.previous_, "Expected ',', 'u' or 'U'");
            }
            auto or_ = AstNode::New(AstNodeType::OR);
            or_->AddNode(std::move(root));
            or_->AddNode(std::move(node));
            root = std::move(or_);
        }
        return root;
    }

    static std::unique_ptr<AstNode> Include(Parser& parser, std::unique_ptr<AstNode> left, bool canAssign) {
        auto contains = AstNode::New(AstNodeType::CONTAINS);
        contains->AddNode(std::move(left));
        auto container = parser.Expression();
        contains->AddNode(std::move(container));
        return contains;
    }

    static std::unique_ptr<AstNode> Call(Parser& parser, std::unique_ptr<AstNode> left, bool canAssign) {
        auto node = AstNode::New(AstNodeType::CALL);
        node->AddNode(std::move(left));

        if (!parser.Check(TokenType::RIGHT_PAREN)) {
            do {
                auto argument = parser.Expression();
                node->AddNode(std::move(argument));
            } while (parser.Match(TokenType::COMMA));
        }

        parser.Consume(TokenType::RIGHT_PAREN, "Expected closing ')'");

        return node;
    }

    static std::unique_ptr<AstNode> Index(Parser& parser, std::unique_ptr<AstNode> left, bool canAssign) {
        auto node = AstNode::New(AstNodeType::INDEX);
        node->AddNode(std::move(left));
        auto index = parser.Expression();
        parser.Consume(TokenType::RIGHT_BRACKET, "Expected closing ']'");
        node->AddNode(std::move(index));

        return node;
    }

    static std::unique_ptr<AstNode> Field(Parser& parser, std::unique_ptr<AstNode> left, bool canAssign) {
        auto node = AstNode::New(AstNodeType::GET);
        node->AddNode(std::move(left));
        parser.Consume(TokenType::IDENTIFIER, "Expected field name");
        auto field = AstNode::New(AstNodeType::NAME, parser.previous_);
        node->AddNode(std::move(field));

        return node;
    }

    static std::unique_ptr<AstNode> Ternary(Parser& parser, std::unique_ptr<AstNode> left, bool canAssign) {
    }

    static std::unique_ptr<AstNode> Sequence(Parser& parser, std::unique_ptr<AstNode> left, bool canAssign) {
        auto node = AstNode::New(AstNodeType::SEQUENCE);
        node->AddNode(std::move(left));
        auto sequence = parser.Expression();
        node->AddNode(std::move(sequence));
        return node;
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
    rules[TokenType::MINUS] = {Expressions::Unary, Expressions::Binary, Precedence::TERM};
    rules[TokenType::STAR] = {Expressions::Unary, Expressions::Binary, Precedence::FACTOR};
    rules[TokenType::STAR_STAR] = {nullptr, Expressions::Binary, Precedence::EXPONENT};
    rules[TokenType::STAR_EQUAL] = {nullptr, nullptr, Precedence::NONE};
    rules[TokenType::SLASH] = {nullptr, Expressions::Binary, Precedence::FACTOR};
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
    while (!Match(TokenType::EOF_)) {
        nodes.push_back(Statement());
    }
    return nodes;
}

Parser::ParseRule Parser::Rule(const TokenType& type) {
    if (rules_.find(type) != rules_.end()) {
        return rules_[type];
    }
    return {};
}

std::unique_ptr<AstNode> Parser::ParsePrecedence(Precedence precedence) {
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

std::unique_ptr<AstNode> Parser::Expression(Precedence start) {
    return ParsePrecedence(start);
}

std::unique_ptr<AstNode> Parser::NodeFromType(const Token& token) {
    switch (token.type) {
        case TokenType::VOID:
            return AstNode::New(AstNodeType::VOID);
        case TokenType::I8:
            return AstNode::New(AstNodeType::I8);
        case TokenType::I16:
            return AstNode::New(AstNodeType::I16);
        case TokenType::I32:
            return AstNode::New(AstNodeType::I32);
        case TokenType::I64:
            return AstNode::New(AstNodeType::I64);
        case TokenType::U8:
            return AstNode::New(AstNodeType::U8);
        case TokenType::U16:
            return AstNode::New(AstNodeType::U16);
        case TokenType::U32:
            return AstNode::New(AstNodeType::U32);
        case TokenType::U64:
            return AstNode::New(AstNodeType::U64);
        case TokenType::F32:
            return AstNode::New(AstNodeType::F32);
        case TokenType::F64:
            return AstNode::New(AstNodeType::F64);
        case TokenType::BOOL:
            return AstNode::New(AstNodeType::BOOL);
        case TokenType::LET:
            return AstNode::New(AstNodeType::INFER);
        default: {
            Error(token, "Unsupported type");
            return nullptr;
        }
    }
}

std::unique_ptr<AstNode> Parser::BuildType(std::unique_ptr<AstNode> base) {
    if (Match(TokenType::LESS)) {
        auto size = Expression(Precedence::SHIFT);
        Consume(TokenType::GREATER, "Expected closing '>'");
        auto simd = AstNode::New(AstNodeType::SIMD);
        simd->AddNode(std::move(base));
        simd->AddNode(std::move(size));
        return BuildType(std::move(simd));
    }
    if (Match(TokenType::STAR)) {
        auto owner = AstNode::New(AstNodeType::OWNER);
        owner->AddNode(std::move(base));
        return BuildType(std::move(owner));
    }
    if (Match(TokenType::AND)) {
        auto borrow = AstNode::New(AstNodeType::BORROW);
        borrow->AddNode(std::move(base));
        return BuildType(std::move(borrow));
    }
    if (Match(TokenType::QUESTION)) {
        auto borrow = AstNode::New(AstNodeType::OPTIONAL);
        borrow->AddNode(std::move(base));
        return BuildType(std::move(borrow));
    }
    if (Match(TokenType::LEFT_BRACKET)) {
        Consume(TokenType::RIGHT_BRACKET, "Expected closing ']'");
        auto dynamic_array = AstNode::New(AstNodeType::ARRAY);
        dynamic_array->AddNode(std::move(base));
        return BuildType(std::move(dynamic_array));
    }
    return base;
}

std::unique_ptr<AstNode> Parser::Statement() {
    if (Match(TokenType::MODULE)) {
        return ModuleStatement();
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

std::unique_ptr<AstNode> Parser::WhileStatement() {
    Consume(TokenType::LEFT_PAREN, "Expected '('");
    auto condition = Expression();
    Consume(TokenType::RIGHT_PAREN, "Expected ')'");
    auto body = Statement();

    auto while_ = AstNode::New(AstNodeType::WHILE);
    while_->AddNode(std::move(condition));
    while_->AddNode(std::move(body));
    return while_;
}

std::unique_ptr<AstNode> Parser::DoWhileStatement() {
    auto body = Statement();
    Consume(TokenType::WHILE, "Expected 'while'");

    Consume(TokenType::LEFT_PAREN, "Expected '('");
    auto condition = Expression();
    Consume(TokenType::RIGHT_PAREN, "Expected ')'");

    auto do_while = AstNode::New(AstNodeType::DO);
    do_while->AddNode(std::move(condition));
    do_while->AddNode(std::move(body));
    return do_while;
}

std::unique_ptr<AstNode> Parser::ForStatement() {
    Consume(TokenType::LEFT_PAREN, "Expected '('");
    auto for_ = AstNode::New(AstNodeType::FOR);
    auto initializer = Statement();
    auto condition = Statement();
    auto increment = Expression();
    Consume(TokenType::RIGHT_PAREN, "Expected ')'");
    auto body = Statement();

    for_->AddNode(std::move(initializer));
    for_->AddNode(std::move(condition));
    for_->AddNode(std::move(increment));
    for_->AddNode(std::move(body));
    return for_;
}

std::unique_ptr<AstNode> Parser::IfStatement() {
    Consume(TokenType::LEFT_PAREN, "Expected '('");
    auto condition = Expression();
    Consume(TokenType::RIGHT_PAREN, "Expected ')'");
    auto then_branch = Statement();

    auto if_ = AstNode::New(AstNodeType::IF);
    if_->AddNode(std::move(condition));
    if_->AddNode(std::move(then_branch));
    if (Match(TokenType::ELSE)) {
        auto else_branch = Statement();
        if_->AddNode(std::move(else_branch));
    }
    return if_;
}

std::unique_ptr<AstNode> Parser::ReturnStatement() {
    auto ret = AstNode::New(AstNodeType::RETURN);
    if (Match(TokenType::SEMICOLON)) {
        return ret;
    }
    auto what = Expression();
    ret->AddNode(std::move(what));
    Consume(TokenType::SEMICOLON, "Expected semicolon");
    return ret;
}

std::unique_ptr<AstNode> Parser::ExpressionStatement() {
    auto expression = Expression();
    Consume(TokenType::SEMICOLON, "Expected semicolon");
    return expression;
}

std::unique_ptr<AstNode> Parser::ModuleStatement() {
    auto node = AstNode::New(AstNodeType::MODULE);
    std::unique_ptr<AstNode> name = nullptr;
    do {
        Consume(TokenType::IDENTIFIER, "Expected identifier");
        if (!name) {
            name = AstNode::New(AstNodeType::NAME, previous_);
        } else {
            auto get = AstNode::New(AstNodeType::GET);
            get->AddNode(std::move(name));
            get->AddNode(AstNode::New(AstNodeType::NAME, previous_));
            name = std::move(get);
        }
    }
    while (Match(TokenType::DOT));

    Consume(TokenType::SEMICOLON, "Expected semicolon after statement");

    node->AddNode(std::move(name));

    return node;
}

std::unique_ptr<AstNode> Parser::Declaration() {
    auto type_token = previous_;
    auto base_node = NodeFromType(type_token);
    auto type_node = BuildType(std::move(base_node));

    Consume(TokenType::IDENTIFIER, "Expected name after definition");
    auto name = AstNode::New(AstNodeType::NAME, previous_);

    if (Match(TokenType::LEFT_PAREN)) {
        auto function = AstNode::New(AstNodeType::FUNCTION);
        function->AddNode(std::move(name));
        function->AddNode(std::move(type_node));

        if (!Check(TokenType::RIGHT_PAREN)) {
            do {
                if (!MatchType())
                    Error(current_, "Expected type");
                function->AddNode(std::move(Declaration()));
            } while (Match(TokenType::COMMA));
        }

        Consume(TokenType::RIGHT_PAREN, "Expected ')'");

        return function;
    }

    auto variable = AstNode::New(AstNodeType::VARIABLE);
    variable->AddNode(std::move(name));
    variable->AddNode(std::move(type_node));
    return variable;
}

std::unique_ptr<AstNode> Parser::BlockStatement() {
    auto sequence = AstNode::New(AstNodeType::GROUP);
    while (!Match(TokenType::RIGHT_BRACE)) {
        sequence->AddNode(Statement());
    }
    return sequence;
}

std::unique_ptr<AstNode> Parser::DeclarationStatement() {
    auto definition = Declaration();
    if (definition->type == AstNodeType::VARIABLE) {
        if (Match(TokenType::EQUAL)) {
            definition->AddNode(std::move(Expression()));
        }
        Consume(TokenType::SEMICOLON, "Expected ';'");
    } else if (definition->type == AstNodeType::FUNCTION) {
        definition->AddNode(Statement());
    }
    return definition;
}

void Parser::Error(const Token& token, const std::string& message) {
    std::cerr << message << " at line: " << token.line << " at: " << token.value << std::endl;
    throw std::exception();
}

Token Parser::Advance() {
    previous_ = current_;
    while (true) {
        current_ = lexer_.Next();
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
