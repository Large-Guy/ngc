//
// Created by ravi on 3/11/26.
//

#include "lexer.h"

std::string TokenTypeToString(TokenType type) {
    switch (type)
    {
        case TokenType::TOKEN_TYPE_ERROR: return "TOKEN_TYPE_ERROR";
        case TokenType::TOKEN_TYPE_EOF: return "TOKEN_TYPE_EOF";
        case TokenType::TOKEN_TYPE_LEFT_PAREN: return "TOKEN_TYPE_LEFT_PAREN";
        case TokenType::TOKEN_TYPE_RIGHT_PAREN: return "TOKEN_TYPE_RIGHT_PAREN";
        case TokenType::TOKEN_TYPE_LEFT_BRACE: return "TOKEN_TYPE_LEFT_BRACE";
        case TokenType::TOKEN_TYPE_RIGHT_BRACE: return "TOKEN_TYPE_RIGHT_BRACE";
        case TokenType::TOKEN_TYPE_LEFT_BRACKET: return "TOKEN_TYPE_LEFT_BRACKET";
        case TokenType::TOKEN_TYPE_RIGHT_BRACKET: return "TOKEN_TYPE_RIGHT_BRACKET";
        case TokenType::TOKEN_TYPE_SEMICOLON: return "TOKEN_TYPE_SEMICOLON";
        case TokenType::TOKEN_TYPE_DOT: return "TOKEN_TYPE_DOT";
        case TokenType::TOKEN_TYPE_COMMA: return "TOKEN_TYPE_COMMA";
        case TokenType::TOKEN_TYPE_PLUS: return "TOKEN_TYPE_PLUS";
        case TokenType::TOKEN_TYPE_PLUS_PLUS: return "TOKEN_TYPE_PLUS_PLUS";
        case TokenType::TOKEN_TYPE_PLUS_EQUAL: return "TOKEN_TYPE_PLUS_EQUAL";
        case TokenType::TOKEN_TYPE_MINUS: return "TOKEN_TYPE_MINUS";
        case TokenType::TOKEN_TYPE_MINUS_MINUS: return "TOKEN_TYPE_MINUS_MINUS";
        case TokenType::TOKEN_TYPE_MINUS_EQUAL: return "TOKEN_TYPE_MINUS_EQUAL";
        case TokenType::TOKEN_TYPE_STAR: return "TOKEN_TYPE_STAR";
        case TokenType::TOKEN_TYPE_STAR_EQUAL: return "TOKEN_TYPE_STAR_EQUAL";
        case TokenType::TOKEN_TYPE_SLASH: return "TOKEN_TYPE_SLASH";
        case TokenType::TOKEN_TYPE_SLASH_EQUAL: return "TOKEN_TYPE_SLASH_EQUAL";
        case TokenType::TOKEN_TYPE_BANG: return "TOKEN_TYPE_BANG";
        case TokenType::TOKEN_TYPE_BANG_EQUAL: return "TOKEN_TYPE_BANG_EQUAL";
        case TokenType::TOKEN_TYPE_EQUAL: return "TOKEN_TYPE_EQUAL";
        case TokenType::TOKEN_TYPE_EQUAL_EQUAL: return "TOKEN_TYPE_EQUAL_EQUAL";
        case TokenType::TOKEN_TYPE_GREATER: return "TOKEN_TYPE_GREATER";
        case TokenType::TOKEN_TYPE_GREATER_EQUAL: return "TOKEN_TYPE_GREATER_EQUAL";
        case TokenType::TOKEN_TYPE_LESS: return "TOKEN_TYPE_LESS";
        case TokenType::TOKEN_TYPE_LESS_EQUAL: return "TOKEN_TYPE_LESS_EQUAL";
        case TokenType::TOKEN_TYPE_COLON: return "TOKEN_TYPE_COLON";
        case TokenType::TOKEN_TYPE_COLON_COLON: return "TOKEN_TYPE_COLON_COLON";
        case TokenType::TOKEN_TYPE_TILDE: return "TOKEN_TYPE_TILDE";
        case TokenType::TOKEN_TYPE_MOVE: return "TOKEN_TYPE_MOVE";
        case TokenType::TOKEN_TYPE_COPY: return "TOKEN_TYPE_COPY";
        case TokenType::TOKEN_TYPE_STRING_LITERAL: return "TOKEN_TYPE_STRING_LITERAL";
        case TokenType::TOKEN_TYPE_INTEGER: return "TOKEN_TYPE_INTEGER";
        case TokenType::TOKEN_TYPE_FLOAT: return "TOKEN_TYPE_FLOATING";
        case TokenType::TOKEN_TYPE_IDENTIFIER: return "TOKEN_TYPE_IDENTIFIER";
        case TokenType::TOKEN_TYPE_MODULE: return "TOKEN_TYPE_MODULE";
        case TokenType::TOKEN_TYPE_IMPORT: return "TOKEN_TYPE_IMPORT";
        case TokenType::TOKEN_TYPE_RETURN: return "TOKEN_TYPE_RETURN";
        case TokenType::TOKEN_TYPE_STRUCT: return "TOKEN_TYPE_STRUCT";
        case TokenType::TOKEN_TYPE_UNION: return "TOKEN_TYPE_UNION";
        case TokenType::TOKEN_TYPE_INTERFACE: return "TOKEN_TYPE_INTERFACE";
        case TokenType::TOKEN_TYPE_STATIC: return "TOKEN_TYPE_STATIC";
        case TokenType::TOKEN_TYPE_REGION: return "TOKEN_TYPE_REGION";
        case TokenType::TOKEN_TYPE_CONST: return "TOKEN_TYPE_CONST";
        case TokenType::TOKEN_TYPE_OPERATOR: return "TOKEN_TYPE_OPERATOR";
        case TokenType::TOKEN_TYPE_UNIQUE: return "TOKEN_TYPE_UNIQUE";
        case TokenType::TOKEN_TYPE_SHARED: return "TOKEN_TYPE_SHARED";
        case TokenType::TOKEN_TYPE_VOID: return "TOKEN_TYPE_VOID";
        case TokenType::TOKEN_TYPE_I8: return "TOKEN_TYPE_I8";
        case TokenType::TOKEN_TYPE_I16: return "TOKEN_TYPE_I16";
        case TokenType::TOKEN_TYPE_I32: return "TOKEN_TYPE_I32";
        case TokenType::TOKEN_TYPE_I64: return "TOKEN_TYPE_I64";
        case TokenType::TOKEN_TYPE_U8: return "TOKEN_TYPE_U8";
        case TokenType::TOKEN_TYPE_U16: return "TOKEN_TYPE_U16";
        case TokenType::TOKEN_TYPE_U32: return "TOKEN_TYPE_U32";
        case TokenType::TOKEN_TYPE_U64: return "TOKEN_TYPE_U64";
        case TokenType::TOKEN_TYPE_F32: return "TOKEN_TYPE_F32";
        case TokenType::TOKEN_TYPE_F64: return "TOKEN_TYPE_F64";
        case TokenType::TOKEN_TYPE_ISIZE: return "TOKEN_TYPE_ISIZE";
        case TokenType::TOKEN_TYPE_USIZE: return "TOKEN_TYPE_USIZE";
        case TokenType::TOKEN_TYPE_STRING: return "TOKEN_TYPE_STRING";
        case TokenType::TOKEN_TYPE_NULL: return "TOKEN_TYPE_NULL";
        case TokenType::TOKEN_TYPE_TRUE: return "TOKEN_TYPE_TRUE";
        case TokenType::TOKEN_TYPE_FALSE: return "TOKEN_TYPE_FALSE";
        case TokenType::TOKEN_TYPE_IF: return "TOKEN_TYPE_IF";
        case TokenType::TOKEN_TYPE_ELSE: return "TOKEN_TYPE_ELSE";
        case TokenType::TOKEN_TYPE_WHILE: return "TOKEN_TYPE_WHILE";
        case TokenType::TOKEN_TYPE_DO: return "TOKEN_TYPE_DO";
        case TokenType::TOKEN_TYPE_FOR: return "TOKEN_TYPE_FOR";
        default: return "unknown";
    }
}

Lexer::Lexer(std::string source) {
    source_ = source;
    line_ = 1;
    start_ = 0;
    current_ = 0;
}

Lexer::~Lexer() = default;

Token Lexer::Next() {
    SkipWhitespace();

    start_ = current_;

    if (End())
        return New(TokenType::TOKEN_TYPE_EOF);

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

        case '(': return New(TokenType::TOKEN_TYPE_LEFT_PAREN);
        case ')': return New(TokenType::TOKEN_TYPE_RIGHT_PAREN);
        case '{': return New(TokenType::TOKEN_TYPE_LEFT_BRACE);
        case '}': return New(TokenType::TOKEN_TYPE_RIGHT_BRACE);
        case '[': return New(TokenType::TOKEN_TYPE_LEFT_BRACKET);
        case ']': return New(TokenType::TOKEN_TYPE_RIGHT_BRACKET);
        case ';': return New(TokenType::TOKEN_TYPE_SEMICOLON);
        case '.': return New(TokenType::TOKEN_TYPE_DOT);
        case ',': return New(TokenType::TOKEN_TYPE_COMMA);

        case ':': return New(Match(':') ? TokenType::TOKEN_TYPE_COLON_COLON : TokenType::TOKEN_TYPE_COLON);
        case '+': return New(Match('=')
                                 ? TokenType::TOKEN_TYPE_PLUS_EQUAL
                                 : Match('+')
                                       ? TokenType::TOKEN_TYPE_PLUS_PLUS
                                       : TokenType::TOKEN_TYPE_PLUS);
        case '-': return New(Match('=')
                                 ? TokenType::TOKEN_TYPE_MINUS_EQUAL
                                 : Match('-')
                                       ? TokenType::TOKEN_TYPE_MINUS_MINUS
                                       : TokenType::TOKEN_TYPE_MINUS);
        case '*': return New(Match('=')
                                 ? TokenType::TOKEN_TYPE_STAR_EQUAL
                                 : Match('*')
                                       ? TokenType::TOKEN_TYPE_STAR_STAR
                                       : TokenType::TOKEN_TYPE_STAR);
        case '?': return New(TokenType::TOKEN_TYPE_QUESTION);
        case '%': return New(Match('=') ? TokenType::TOKEN_TYPE_PERCENT_EQUAL : TokenType::TOKEN_TYPE_PERCENT);
        case '/': return New(Match('=') ? TokenType::TOKEN_TYPE_SLASH_EQUAL : TokenType::TOKEN_TYPE_SLASH);
        case '^': return New(Match('=') ? TokenType::TOKEN_TYPE_CARET_EQUAL : TokenType::TOKEN_TYPE_CARET);
        case '=': return New(Match('=') ? TokenType::TOKEN_TYPE_EQUAL_EQUAL : TokenType::TOKEN_TYPE_EQUAL);
        case '!': return New(Match('=') ? TokenType::TOKEN_TYPE_BANG_EQUAL : TokenType::TOKEN_TYPE_BANG);
        case '<': return New(Match('=')
                                 ? TokenType::TOKEN_TYPE_LESS_EQUAL
                                 : Match('<')
                                       ? TokenType::TOKEN_TYPE_LESS_LESS
                                       : TokenType::TOKEN_TYPE_LESS);
        case '>': return New(Match('=')
                                 ? TokenType::TOKEN_TYPE_GREATER_EQUAL
                                 : Match('>')
                                       ? TokenType::TOKEN_TYPE_GREATER_GREATER
                                       : TokenType::TOKEN_TYPE_GREATER);
        case '~': return New(Match('=') ? TokenType::TOKEN_TYPE_TILDE_EQUAL : TokenType::TOKEN_TYPE_TILDE);

        case '&': return New(Match('&')
                                 ? TokenType::TOKEN_TYPE_AND_AND
                                 : Match('=')
                                       ? TokenType::TOKEN_TYPE_AND_EQUAL
                                       : TokenType::TOKEN_TYPE_AND);
        case '|': return New(Match('|')
                                 ? TokenType::TOKEN_TYPE_PIPE_PIPE
                                 : Match('=')
                                       ? TokenType::TOKEN_TYPE_PIPE_EQUAL
                                       : TokenType::TOKEN_TYPE_PIPE);
        default:
            break;
    }

    return New(TokenType::TOKEN_TYPE_ERROR);
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
                }
                else {
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
        return New(TokenType::TOKEN_TYPE_ERROR);
    Advance();
    return New(TokenType::TOKEN_TYPE_STRING_LITERAL);
}

Token Lexer::Character() {
    Advance();
    Match('\'');
    return New(TokenType::TOKEN_TYPE_CHARACTER);
}

Token Lexer::Number() {
    bool floating = false;

    while (IsDigit(Peek())) {
        Advance();
    }

    if (Peek() == '.' && IsDigit(PeekNext())) {
        floating = true;
        Advance();
        while (IsDigit(Peek())) {
            Advance();
        }
    }

    if (Peek() == 'f' || Peek() == 'F') {
        floating = true;
        Advance();
    }

    return New(floating ? TokenType::TOKEN_TYPE_FLOAT : TokenType::TOKEN_TYPE_INTEGER);
}

TokenType Lexer::Keyword(uint32_t start, std::string remainder, TokenType type) {
    if (current_ - start_ == start + remainder.size() && source_.substr(start, remainder.size()) == remainder) {
        return type;
    }
    return TokenType::TOKEN_TYPE_IDENTIFIER;
}

TokenType Lexer::Type() {
    switch (Peek()) {
        case 'r':
            {
                if (current_ - start_ > 1 && PeekNext() == 'e' && current_ - start_ > 2)
                {
                    switch (PeekNext(2))
                    {
                        case 'g': return Keyword(3, "ion", TokenType::TOKEN_TYPE_REGION);
                        case 't': return Keyword(3, "urn", TokenType::TOKEN_TYPE_RETURN);
                        default: break;
                    }
                }
                break;
            }
        case 'v': return Keyword(1, "oid", TokenType::TOKEN_TYPE_VOID);
        case 'i':
            {
                if (current_ - start_ > 1)
                {
                    switch (PeekNext())
                    {
                        case '8': return Keyword(2, "", TokenType::TOKEN_TYPE_I8);
                        case '1': return Keyword(2, "6", TokenType::TOKEN_TYPE_I16);
                        case '3': return Keyword(2, "2", TokenType::TOKEN_TYPE_I32);
                        case '6': return Keyword(2, "4", TokenType::TOKEN_TYPE_I64);
                        case 's': return Keyword(2, "ize", TokenType::TOKEN_TYPE_ISIZE);
                        case 'm': return Keyword(2, "port", TokenType::TOKEN_TYPE_IMPORT);
                        case 'f': return Keyword(2, "", TokenType::TOKEN_TYPE_IF);
                        case 'n': {
                            if (current_ - start_ > 2) {
                                switch (PeekNext()) {
                                    case 't': return Keyword(3, "erface", TokenType::TOKEN_TYPE_INTERFACE);
                                }
                            }
                            else {
                                return TokenType::TOKEN_TYPE_IN;
                            }
                        }
                        default: break;
                    }
                }
                break;
            }
        case 'u':
            {
                if (current_ - start_ > 1)
                {
                    switch (PeekNext())
                    {
                        case '8': return Keyword(2, "", TokenType::TOKEN_TYPE_U8);
                        case '1': return Keyword(2, "6", TokenType::TOKEN_TYPE_U16);
                        case '3': return Keyword(2, "2", TokenType::TOKEN_TYPE_U32);
                        case '6': return Keyword(2, "4", TokenType::TOKEN_TYPE_U64);
                        case 's': return Keyword(2, "ize", TokenType::TOKEN_TYPE_USIZE);
                        case 'n': return Keyword(2, "ion", TokenType::TOKEN_TYPE_UNION);
                        default: break;
                    }
                }
                break;
            }
        case 's':
            {
                if (current_ - start_ > 1 && PeekNext() == 't' && current_ - start_ > 2)
                {
                    switch (PeekNext(2))
                    {
                        case 'r':
                            {
                                if (current_ - start_ > 3)
                                {
                                    switch (PeekNext(3))
                                    {
                                        case 'i': return Keyword(4, "ng", TokenType::TOKEN_TYPE_STRING);
                                        case 'u': return Keyword(4, "ct", TokenType::TOKEN_TYPE_STRUCT);
                                        default: break;
                                    }
                                }
                                break;
                            }
                        case 'a': return Keyword(3, "tic", TokenType::TOKEN_TYPE_STATIC);
                        default: break;
                    }
                }
                break;
            }
        case 'm':
            {
                if (current_ - start_ > 1 && PeekNext() == 'o' && current_ - start_ > 2)
                {
                    switch (PeekNext(2))
                    {
                        case 'v': return Keyword(3, "e", TokenType::TOKEN_TYPE_MOVE);
                        case 'd': return Keyword(3, "ule", TokenType::TOKEN_TYPE_MODULE);
                        default: break;
                    }
                }
                break;
            }
        case 'c':
            {
                if (current_ - start_ > 1 && PeekNext() == 'o' && current_ - start_ > 2)
                {
                    switch (PeekNext(2))
                    {
                        case 'p': return Keyword(3, "y", TokenType::TOKEN_TYPE_COPY);
                        case 'n': return Keyword(3, "st", TokenType::TOKEN_TYPE_CONST);
                        default: break;
                    }
                }
                break;
            }
        case 'n': return Keyword(1, "ull", TokenType::TOKEN_TYPE_NULL);
        case 't': return Keyword(1, "rue", TokenType::TOKEN_TYPE_TRUE);
        case 'f':
            {
                if (current_ - start_ > 1)
                {
                    switch (PeekNext())
                    {
                        case 'a': return Keyword(2, "lse", TokenType::TOKEN_TYPE_FALSE);
                        case 'o': return Keyword(2, "r", TokenType::TOKEN_TYPE_FOR);
                        case '3': return Keyword(2, "2", TokenType::TOKEN_TYPE_F32);
                        case '6': return Keyword(2, "4", TokenType::TOKEN_TYPE_F64);
                        default: break;
                    }
                }
            }
        case 'e': return Keyword(1, "lse", TokenType::TOKEN_TYPE_ELSE);
        case 'w': return Keyword(1, "hile", TokenType::TOKEN_TYPE_WHILE);
        case 'd': return Keyword(1, "o", TokenType::TOKEN_TYPE_DO);
        case 'o': return Keyword(1, "perator", TokenType::TOKEN_TYPE_OPERATOR);
        case 'l': return Keyword(1, "et", TokenType::TOKEN_TYPE_LET);
        case 'b': return Keyword(1, "ool", TokenType::TOKEN_TYPE_BOOL);
        default: break;
    }
    return TokenType::TOKEN_TYPE_IDENTIFIER;
}

Token Lexer::Identifier() {
    while (IsAlpha(Peek()) || IsDigit(Peek())) {
        Advance();
    }
    return New(Type());
}
