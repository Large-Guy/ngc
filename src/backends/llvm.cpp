#include "llvm.h"

#include <iostream>

Type * LlvmBackend::CreateType(AstNode *ast_node) {
    switch (ast_node->type) {
        case AstNodeType::VOID: return Type::getVoidTy(*context_);
        case AstNodeType::I8: return Type::getInt8Ty(*context_);
        case AstNodeType::I16: return Type::getInt16Ty(*context_);
        case AstNodeType::I32: return Type::getInt32Ty(*context_);
        case AstNodeType::I64: return Type::getInt64Ty(*context_);
        case AstNodeType::U8: return Type::getInt8Ty(*context_);
        case AstNodeType::U16: return Type::getInt16Ty(*context_);
        case AstNodeType::U32: return Type::getInt32Ty(*context_);
        case AstNodeType::U64: return Type::getInt64Ty(*context_);
        case AstNodeType::F32: return Type::getFloatTy(*context_);
        case AstNodeType::F64: return Type::getDoubleTy(*context_);
        case AstNodeType::BOOL: return Type::getInt1Ty(*context_);
        default: {
            std::cerr << "Unsupported type" << std::endl;
            break;
        }
    }
}

void LlvmBackend::Block(Function* function, AstNode *body) {
    BasicBlock* block = BasicBlock::Create(*context_, "entry", function);
    builder_->SetInsertPoint(block);
}

void LlvmBackend::Declaration(const AstNode &node) {
    switch (node.type) {
        case AstNodeType::MODULE: {
            const auto name = node[0];
            if (module_ != nullptr) {
                std::cout << "Module already generated" << std::endl;
            }
            module_ = std::make_unique<Module>(name->token->value, *context_);
            std::cout << "Module generated" << std::endl;
            break;
        }
        case AstNodeType::FUNCTION: {
            const auto name = node[0];
            const auto return_type = node[1];
            std::vector<Type*> args;
            std::vector<std::string> arg_names;
            for (auto i = 2; i < node.ChildrenCount() - 1; ++i) {
                auto& arg = *node[i];
                args.push_back(CreateType(arg[1]));
                arg_names.push_back(arg[0]->token->value);
            }

            FunctionType* function_type = FunctionType::get(CreateType(return_type), args, false);

            Function* function = Function::Create(function_type, Function::ExternalLinkage, name->token->value, module_.get());

            unsigned idx = 0;
            for (auto& arg : function->args()) {
                arg.setName(arg_names[idx++]);
            }

            const auto body = node[node.ChildrenCount() - 1];

            Block(function, body);
            break;
        }
        default: std::cerr << "Unsupported node type" << std::endl;
    }
}

void LlvmBackend::Generate(std::vector<std::unique_ptr<AstNode>> nodes) {
    context_ = std::make_unique<LLVMContext>();
    builder_ = std::make_unique<IRBuilder<>>(*context_);

    for (const auto& ptr : nodes) {
        AstNode& node = *ptr.get();
        Declaration(node);
    }

    module_->print(llvm::outs(), nullptr);
}

