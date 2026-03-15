//
// Created by ravi on 3/11/26.
//

#ifndef NCC_LEXER_H
#define NCC_LEXER_H
#include <cstdint>
#include <string>

enum class TokenType {
    ERROR,
    EOF_,

    //Operators
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    SEMICOLON,
    DOT,
    COMMA,
    PLUS,
    PLUS_PLUS,
    PLUS_EQUAL,
    MINUS,
    MINUS_MINUS,
    MINUS_EQUAL,
    STAR,
    STAR_STAR,
    STAR_EQUAL,
    SLASH,
    SLASH_EQUAL,
    BANG,
    BANG_EQUAL,
    EQUAL,
    EQUAL_EQUAL,
    GREATER,
    GREATER_GREATER,
    GREATER_EQUAL,
    LESS,
    LESS_LESS,
    LESS_EQUAL,
    COLON,
    COLON_COLON,
    AND,
    AND_EQUAL,
    AND_AND,
    PIPE,
    PIPE_EQUAL,
    PIPE_PIPE,
    PERCENT,
    PERCENT_EQUAL,
    CARET,
    CARET_EQUAL,
    TILDE,
    TILDE_EQUAL,
    QUESTION,

    IN,

    //Special
    MOVE,
    COPY,

    STRING_LITERAL,
    CHARACTER,
    INTEGER,
    FLOAT,

    IDENTIFIER,

    //KEYWORDS
    MODULE,
    IMPORT,

    REGION,

    RETURN,

    STRUCT,
    UNION,
    INTERFACE,

    //Qualifiers
    STATIC,
    CONST,
    OPERATOR,

    //i'm not 100% sure what the syntax will look like for this
    UNIQUE,
    SHARED,

    //Types
    LET,

    VOID,

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

    ISIZE,
    USIZE,

    STRING,

    BOOL,

    //constants
    NULL_,
    TRUE,
    FALSE,

    //Branching
    IF,
    ELSE,
    WHILE,
    DO,
    FOR,

    COUNT
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