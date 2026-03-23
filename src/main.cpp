#include <fstream>
#include <iostream>

#include "lexer.h"
#include "parser.h"
#include "backends/llvm_backend.h"

int main() {
    std::string path = "main.n";
    std::ifstream file(path);
    auto file_size = file.seekg(0, std::ios::end).tellg();
    file.seekg(0);

    std::string content(file_size, '\0');
    file.read(&content[0], file_size);
    file.close();

    Lexer lexer = {content};

    Parser parser = {std::move(lexer)};

    auto nodes = parser.Parse();

    for (auto& node: nodes) {
        //node->Debug();
    }

    LLVMBackend backend = {};

    backend.Generate(std::move(nodes));

    return 0;
}
