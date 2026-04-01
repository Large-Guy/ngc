#include <fstream>
#include <iostream>

#include "lexer.h"
#include "parser.h"
#include "backends/llvm_backend.h"

int main(int argc, char** argv) {
    std::vector<Lexer> lexers;

    for (auto i = 1; i < argc; i++) {
        std::string path = argv[i];
        std::ifstream file(path);
        auto file_size = file.seekg(0, std::ios::end).tellg();
        file.seekg(0);

        std::string content(file_size, '\0');
        file.read(&content[0], file_size);
        file.close();

        lexers.emplace_back(content);
    }

    Parser parser = {lexers};

    auto nodes = parser.Parse();

    for (auto& node: nodes) {
        //node->Debug();
    }

    LLVMBackend backend = {};

    backend.Generate(std::move(nodes));

    system("gcc output.o -o output");
    system("./output || echo \"Program finished with exit code\" $?");

    return 0;
}
