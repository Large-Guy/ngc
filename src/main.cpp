#include <fstream>
#include <iostream>

#include "ast_node.h"
#include "lexer.h"

int main() {
    std::string path = "main.n";
    std::ifstream file(path);
    auto file_size = file.seekg(0, std::ios::end).tellg();
    file.seekg(0);

    std::string content(file_size, '\0');
    file.read(&content[0], file_size);
    file.close();

    Lexer lexer = {content};

    while (true) {
        auto token = lexer.Next();
        if (token.type == TokenType::TOKEN_TYPE_EOF)
            break;
        std::cout << TokenTypeToString(token.type) << " " << token.value << std::endl;
    }


    return 0;
}
