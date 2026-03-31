//
// Created by ravi on 3/11/26.
//

#include "lexer.h"

std::string TokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::ERROR: return "ERROR";
        case TokenType::EOF_: return "EOF";
        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::LEFT_BRACKET: return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::DOT: return "DOT";
        case TokenType::COMMA: return "COMMA";
        case TokenType::PLUS: return "PLUS";
        case TokenType::PLUS_PLUS: return "PLUS_PLUS";
        case TokenType::PLUS_EQUAL: return "PLUS_EQUAL";
        case TokenType::MINUS: return "MINUS";
        case TokenType::MINUS_MINUS: return "MINUS_MINUS";
        case TokenType::MINUS_EQUAL: return "MINUS_EQUAL";
        case TokenType::STAR: return "STAR";
        case TokenType::STAR_EQUAL: return "STAR_EQUAL";
        case TokenType::SLASH: return "SLASH";
        case TokenType::SLASH_EQUAL: return "SLASH_EQUAL";
        case TokenType::BANG: return "BANG";
        case TokenType::BANG_EQUAL: return "BANG_EQUAL";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::COLON: return "COLON";
        case TokenType::COLON_COLON: return "COLON_COLON";
        case TokenType::TILDE: return "TILDE";
        case TokenType::MOVE: return "MOVE";
        case TokenType::COPY: return "COPY";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::INTEGER: return "INTEGER";
        case TokenType::FLOAT: return "FLOATING";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::MODULE: return "MODULE";
        case TokenType::IMPORT: return "IMPORT";
        case TokenType::RETURN: return "RETURN";
        case TokenType::STRUCT: return "STRUCT";
        case TokenType::UNION: return "UNION";
        case TokenType::INTERFACE: return "INTERFACE";
        case TokenType::STATIC: return "STATIC";
        case TokenType::REGION: return "REGION";
        case TokenType::CONST: return "CONST";
        case TokenType::OPERATOR: return "OPERATOR";
        case TokenType::UNIQUE: return "UNIQUE";
        case TokenType::SHARED: return "SHARED";
        case TokenType::VOID: return "VOID";
        case TokenType::I8: return "I8";
        case TokenType::I16: return "I16";
        case TokenType::I32: return "I32";
        case TokenType::I64: return "I64";
        case TokenType::U8: return "U8";
        case TokenType::U16: return "U16";
        case TokenType::U32: return "U32";
        case TokenType::U64: return "U64";
        case TokenType::F32: return "F32";
        case TokenType::F64: return "F64";
        case TokenType::ISIZE: return "ISIZE";
        case TokenType::USIZE: return "USIZE";
        case TokenType::STRING: return "STRING";
        case TokenType::LET: return "LET";
        case TokenType::NULL_: return "NULL";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::DO: return "DO";
        case TokenType::FOR: return "FOR";
        default: return "unknown";
    }
}

Lexer::Lexer(std::string source) {
    source_ = source;
    line_ = 1;
    start_ = 0;
    current_ = 0;
}

Lexer::Lexer(Lexer& lexer) noexcept {
    source_ = lexer.source_;
    line_ = lexer.line_;
    start_ = lexer.start_;
    current_ = lexer.current_;
}

Lexer::Lexer(Lexer&& lexer) noexcept {
    source_ = std::move(lexer.source_);
    line_ = lexer.line_;
    start_ = lexer.start_;
    current_ = lexer.current_;
}

Lexer::~Lexer() = default;

Token Lexer::Next() {
    SkipWhitespace();

    start_ = current_;

    if (End())
        return New(TokenType::EOF_);

    char c = Advance();

    if (IsAlpha(c)) {
        return Identifier();
    }

    if (IsDigit(c)) {
        return Number();
    }

    switch (c) {
        case '"': return String();
        case '\'': return Character();

        case '(': return New(TokenType::LEFT_PAREN);
        case ')': return New(TokenType::RIGHT_PAREN);
        case '{': return New(TokenType::LEFT_BRACE);
        case '}': return New(TokenType::RIGHT_BRACE);
        case '[': return New(TokenType::LEFT_BRACKET);
        case ']': return New(TokenType::RIGHT_BRACKET);
        case ';': return New(TokenType::SEMICOLON);
        case '.': return New(Match('.') ? TokenType::DOT_DOT : TokenType::DOT);
        case ',': return New(TokenType::COMMA);

        case ':': return New(Match(':') ? TokenType::COLON_COLON : TokenType::COLON);
        case '+': return New(Match('=')
                                 ? TokenType::PLUS_EQUAL
                                 : Match('+')
                                       ? TokenType::PLUS_PLUS
                                       : TokenType::PLUS);
        case '-': return New(Match('=')
                                 ? TokenType::MINUS_EQUAL
                                 : Match('-')
                                       ? TokenType::MINUS_MINUS
                                       : TokenType::MINUS);
        case '*': return New(Match('=')
                                 ? TokenType::STAR_EQUAL
                                 : Match('*')
                                       ? TokenType::STAR_STAR
                                       : TokenType::STAR);
        case '?': return New(TokenType::QUESTION);
        case '%': return New(Match('=') ? TokenType::PERCENT_EQUAL : TokenType::PERCENT);
        case '/': return New(Match('=') ? TokenType::SLASH_EQUAL : TokenType::SLASH);
        case '^': return New(Match('=') ? TokenType::CARET_EQUAL : TokenType::CARET);
        case '=': return New(Match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
        case '!': return New(Match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
        case '<': return New(Match('=')
                                 ? TokenType::LESS_EQUAL
                                 : Match('<')
                                       ? TokenType::LESS_LESS
                                       : TokenType::LESS);
        case '>': return New(Match('=')
                                 ? TokenType::GREATER_EQUAL
                                 : Match('>')
                                       ? TokenType::GREATER_GREATER
                                       : TokenType::GREATER);
        case '~': return New(Match('=') ? TokenType::TILDE_EQUAL : TokenType::TILDE);

        case '&': return New(Match('&')
                                 ? TokenType::AND_AND
                                 : Match('=')
                                       ? TokenType::AND_EQUAL
                                       : TokenType::AND);
        case '|': return New(Match('|')
                                 ? TokenType::PIPE_PIPE
                                 : Match('=')
                                       ? TokenType::PIPE_EQUAL
                                       : TokenType::PIPE);
        default:
            break;
    }

    return New(TokenType::ERROR);
}

bool Lexer::End() {
    return source_[current_] == '\0';
}

Token Lexer::New(TokenType type) {
    return Token{type, source_.substr(start_, current_ - start_), line_};
}

char Lexer::Advance() {
    return source_[current_++];
}

bool Lexer::Match(char expected) {
    if (End())
        return false;
    if (source_[current_] != expected)
        return false;
    current_++;
    return true;
}

char Lexer::View(uint32_t offset) {
    return source_[start_ + offset];
}

char Lexer::Peek() {
    return source_[current_];
}

char Lexer::PeekNext() {
    if (End())
        return '\0';
    return source_[current_ + 1];
}

char Lexer::PeekNext(uint32_t offset) {
    if (current_ + offset >= source_.size())
        return '\0';
    return source_[current_ + offset];
}

void Lexer::SkipWhitespace() {
    while (true) {
        char c = Peek();
        switch (c) {
            case '/': {
                if (PeekNext() == '/') {
                    while (Peek() != '\n' && !End())
                        Advance();
                } else {
                    return;
                }
            }
            case '\n':
            case '\r':
                line_++;
            case ' ':
            case '\t':
                Advance();
                break;
            default:
                return;
        }
    }
}

bool Lexer::IsDigit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::IsAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

Token Lexer::String() {
    while (Peek() != '"' && !End()) {
        if (Peek() == '\n')
            line_++;
        Advance();
    }
    if (End())
        return New(TokenType::ERROR);
    Advance();
    return New(TokenType::STRING_LITERAL);
}

Token Lexer::Character() {
    Advance();
    Match('\'');
    return New(TokenType::CHARACTER);
}

Token Lexer::Number() {
    int floating = 0;

    while (IsDigit(Peek())) {
        Advance();
    }

    if (Peek() == '.' && IsDigit(PeekNext())) {
        floating = 1;
        Advance();
        while (IsDigit(Peek())) {
            Advance();
        }
    }

    if (Peek() == 'f' || Peek() == 'F') {
        floating = 1;
        Advance();
        if (Peek() == 'f' || Peek() == 'F') {
            floating = 2;
            Advance();
        }
    }

    return New(floating ? (floating == 1 ? TokenType::FLOAT : TokenType::DOUBLE) : TokenType::INTEGER);
}

TokenType Lexer::Keyword(uint32_t start, std::string remainder, TokenType type) {
    auto sub = source_.substr(start, remainder.size());
    if (current_ - start_ == start + remainder.size() && source_.substr(start_ + start, remainder.size()) ==
        remainder) {
        return type;
    }
    return TokenType::IDENTIFIER;
}

TokenType Lexer::Type() {
    switch (View()) {
        case 'r': {
            if (current_ - start_ > 1 && View(1) == 'e' && current_ - start_ > 2) {
                switch (View(2)) {
                    case 'g': return Keyword(3, "ion", TokenType::REGION);
                    case 't': return Keyword(3, "urn", TokenType::RETURN);
                    default: break;
                }
            }
            break;
        }
        case 'v': return Keyword(1, "oid", TokenType::VOID);
        case 'i': {
            if (current_ - start_ > 1) {
                switch (View(1)) {
                    case '8': return Keyword(2, "", TokenType::I8);
                    case '1': return Keyword(2, "6", TokenType::I16);
                    case '3': return Keyword(2, "2", TokenType::I32);
                    case '6': return Keyword(2, "4", TokenType::I64);
                    case 's': return Keyword(2, "ize", TokenType::ISIZE);
                    case 'm': return Keyword(2, "port", TokenType::IMPORT);
                    case 'f': return Keyword(2, "", TokenType::IF);
                    case 'n': {
                        if (current_ - start_ > 2) {
                            switch (View(2)) {
                                case 't': return Keyword(3, "erface", TokenType::INTERFACE);
                            }
                        } else {
                            return TokenType::IN;
                        }
                    }
                    default: break;
                }
            }
            break;
        }
        case 'u': {
            if (current_ - start_ > 1) {
                switch (View(1)) {
                    case '8': return Keyword(2, "", TokenType::U8);
                    case '1': return Keyword(2, "6", TokenType::U16);
                    case '3': return Keyword(2, "2", TokenType::U32);
                    case '6': return Keyword(2, "4", TokenType::U64);
                    case 's': return Keyword(2, "ize", TokenType::USIZE);
                    case 'n': return Keyword(2, "ion", TokenType::UNION);
                    default: break;
                }
            }
            break;
        }
        case 's': {
            if (current_ - start_ > 1 && View(1) == 't' && current_ - start_ > 2) {
                switch (View(2)) {
                    case 'r': {
                        if (current_ - start_ > 3) {
                            switch (View(3)) {
                                case 'i': return Keyword(4, "ng", TokenType::STRING);
                                case 'u': return Keyword(4, "ct", TokenType::STRUCT);
                                default: break;
                            }
                        }
                        break;
                    }
                    case 'a': return Keyword(3, "tic", TokenType::STATIC);
                    default: break;
                }
            }
            break;
        }
        case 'm': {
            if (current_ - start_ > 1 && View(1) == 'o' && current_ - start_ > 2) {
                switch (View(2)) {
                    case 'v': return Keyword(3, "e", TokenType::MOVE);
                    case 'd': return Keyword(3, "ule", TokenType::MODULE);
                    default: break;
                }
            }
            break;
        }
        case 'c': {
            if (current_ - start_ > 1 && View(1) == 'o' && current_ - start_ > 2) {
                switch (View(2)) {
                    case 'p': return Keyword(3, "y", TokenType::COPY);
                    case 'n': return Keyword(3, "st", TokenType::CONST);
                    default: break;
                }
            }
            break;
        }
        case 'n': return Keyword(1, "ull", TokenType::NULL_);
        case 't': return Keyword(1, "rue", TokenType::TRUE);
        case 'f': {
            if (current_ - start_ > 1) {
                switch (View(1)) {
                    case 'a': return Keyword(2, "lse", TokenType::FALSE);
                    case 'o': return Keyword(2, "r", TokenType::FOR);
                    case '3': return Keyword(2, "2", TokenType::F32);
                    case '6': return Keyword(2, "4", TokenType::F64);
                    default: break;
                }
            }
        }
        case 'e': return Keyword(1, "lse", TokenType::ELSE);
        case 'w': return Keyword(1, "hile", TokenType::WHILE);
        case 'd': return Keyword(1, "o", TokenType::DO);
        case 'o': return Keyword(1, "perator", TokenType::OPERATOR);
        case 'l': return Keyword(1, "et", TokenType::LET);
        case 'b': return Keyword(1, "ool", TokenType::BOOL);
        case 'a': return Keyword(1, "s", TokenType::AS);
        default: break;
    }
    return TokenType::IDENTIFIER;
}

Token Lexer::Identifier() {
    while (IsAlpha(Peek()) || IsDigit(Peek())) {
        Advance();
    }
    return New(Type());
}
