//
// Created by ravi on 3/11/26.
//

#ifndef NCC_LEXER_H
#define NCC_LEXER_H
#include <cstdint>
#include <string>

enum class TokenType {
    TOKEN_TYPE_ERROR,
    TOKEN_TYPE_EOF,

    //Operators
    TOKEN_TYPE_LEFT_PAREN,
    TOKEN_TYPE_RIGHT_PAREN,
    TOKEN_TYPE_LEFT_BRACE,
    TOKEN_TYPE_RIGHT_BRACE,
    TOKEN_TYPE_LEFT_BRACKET,
    TOKEN_TYPE_RIGHT_BRACKET,
    TOKEN_TYPE_SEMICOLON,
    TOKEN_TYPE_DOT,
    TOKEN_TYPE_COMMA,
    TOKEN_TYPE_PLUS,
    TOKEN_TYPE_PLUS_PLUS,
    TOKEN_TYPE_PLUS_EQUAL,
    TOKEN_TYPE_MINUS,
    TOKEN_TYPE_MINUS_MINUS,
    TOKEN_TYPE_MINUS_EQUAL,
    TOKEN_TYPE_STAR,
    TOKEN_TYPE_STAR_STAR,
    TOKEN_TYPE_STAR_EQUAL,
    TOKEN_TYPE_SLASH,
    TOKEN_TYPE_SLASH_EQUAL,
    TOKEN_TYPE_BANG,
    TOKEN_TYPE_BANG_EQUAL,
    TOKEN_TYPE_EQUAL,
    TOKEN_TYPE_EQUAL_EQUAL,
    TOKEN_TYPE_GREATER,
    TOKEN_TYPE_GREATER_GREATER,
    TOKEN_TYPE_GREATER_EQUAL,
    TOKEN_TYPE_LESS,
    TOKEN_TYPE_LESS_LESS,
    TOKEN_TYPE_LESS_EQUAL,
    TOKEN_TYPE_COLON,
    TOKEN_TYPE_COLON_COLON,
    TOKEN_TYPE_AND,
    TOKEN_TYPE_AND_EQUAL,
    TOKEN_TYPE_AND_AND,
    TOKEN_TYPE_PIPE,
    TOKEN_TYPE_PIPE_EQUAL,
    TOKEN_TYPE_PIPE_PIPE,
    TOKEN_TYPE_PERCENT,
    TOKEN_TYPE_PERCENT_EQUAL,
    TOKEN_TYPE_CARET,
    TOKEN_TYPE_CARET_EQUAL,
    TOKEN_TYPE_TILDE,
    TOKEN_TYPE_TILDE_EQUAL,
    TOKEN_TYPE_QUESTION,

    TOKEN_TYPE_IN,

    //Special
    TOKEN_TYPE_MOVE,
    TOKEN_TYPE_COPY,

    TOKEN_TYPE_STRING_LITERAL,
    TOKEN_TYPE_CHARACTER,
    TOKEN_TYPE_INTEGER,
    TOKEN_TYPE_FLOAT,

    TOKEN_TYPE_IDENTIFIER,

    //KEYWORDS
    TOKEN_TYPE_MODULE,
    TOKEN_TYPE_IMPORT,

    TOKEN_TYPE_REGION,

    TOKEN_TYPE_RETURN,

    TOKEN_TYPE_STRUCT,
    TOKEN_TYPE_UNION,
    TOKEN_TYPE_INTERFACE,

    //Qualifiers
    TOKEN_TYPE_STATIC,
    TOKEN_TYPE_CONST,
    TOKEN_TYPE_OPERATOR,

    //i'm not 100% sure what the syntax will look like for this
    TOKEN_TYPE_UNIQUE,
    TOKEN_TYPE_SHARED,

    //Types
    TOKEN_TYPE_LET,

    TOKEN_TYPE_VOID,

    TOKEN_TYPE_I8,
    TOKEN_TYPE_I16,
    TOKEN_TYPE_I32,
    TOKEN_TYPE_I64,

    TOKEN_TYPE_U8,
    TOKEN_TYPE_U16,
    TOKEN_TYPE_U32,
    TOKEN_TYPE_U64,

    TOKEN_TYPE_F32,
    TOKEN_TYPE_F64,

    TOKEN_TYPE_ISIZE,
    TOKEN_TYPE_USIZE,

    TOKEN_TYPE_STRING,

    TOKEN_TYPE_BOOL,

    //constants
    TOKEN_TYPE_NULL,
    TOKEN_TYPE_TRUE,
    TOKEN_TYPE_FALSE,

    //Branching
    TOKEN_TYPE_IF,
    TOKEN_TYPE_ELSE,
    TOKEN_TYPE_WHILE,
    TOKEN_TYPE_DO,
    TOKEN_TYPE_FOR,

    TOKEN_TYPE_COUNT
};

std::string TokenTypeToString(TokenType type);

struct Token {
    TokenType type;
    std::string value;
    uint32_t line;
};

class Lexer {
public:

    Lexer(std::string source);
    Lexer(Lexer& lexer) noexcept;
    Lexer(Lexer&& lexer) noexcept;
    ~Lexer();

    Token Next();

private:
    bool End();
    Token New(TokenType type);
    char Advance();
    bool Match(char expected);
    char View(uint32_t offset = 0);
    char Peek();
    char PeekNext();
    char PeekNext(uint32_t offset);
    void SkipWhitespace();
    bool IsDigit(char c);
    bool IsAlpha(char c);
    Token String();
    Token Character();
    Token Number();

    TokenType Keyword(uint32_t start, std::string remainder, TokenType type);
    TokenType Type();
    Token Identifier();


    std::string source_;
    uint32_t line_;
    uint32_t start_;
    uint32_t current_;
};


#endif //NCC_LEXER_H