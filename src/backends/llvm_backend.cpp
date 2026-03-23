#include "llvm_backend.h"

#include <iostream>

#include "../ast/nodes/assign_node.h"
#include "../ast/nodes/binary_node.h"
#include "../ast/nodes/compound_statement.h"
#include "../ast/nodes/float_node.h"
#include "../ast/nodes/function_node.h"
#include "../ast/nodes/identifier_node.h"
#include "../ast/nodes/integer_node.h"
#include "../ast/nodes/module_node.h"
#include "../ast/nodes/return_node.h"
#include "../ast/nodes/variable_node.h"

void LLVMBackend::Generate(std::vector<std::unique_ptr<AstNode> > nodes) {
    context_ = std::make_unique<LLVMContext>();
    builder_ = std::make_unique<IRBuilder<> >(*context_);

    for (const auto &node: nodes) {
        if (const auto module = is<ModuleNode>(node.get())) {
            module_ = std::make_unique<Module>(module->path, *context_);
            std::cout << "Module: " << module->path << std::endl;
            continue;
        }
        if (const auto function = is<FunctionNode>(node.get())) {
            GenerateFunction(function);
        }
    }

    module_->print(llvm::outs(), nullptr);
}

int64_t LLVMBackend::Evaluate(ExpressionNode *unique) {
    throw std::runtime_error("Not implemented");
}

Type *LLVMBackend::GenerateType(TypeNode *type) {
    switch (type->type) {
        case TypeNodeType::VOID:
            return Type::getVoidTy(*context_);
        case TypeNodeType::BORROW:
            return PointerType::get(*context_, 0);
        case TypeNodeType::OWNER:
            return PointerType::get(*context_, 0);
        case TypeNodeType::OPTIONAL:
            throw std::runtime_error("Optional types are not supported");
        case TypeNodeType::ARRAY:
            throw std::runtime_error("Array types are not supported");
        case TypeNodeType::MAP:
            throw std::runtime_error("Map types are not supported");
        case TypeNodeType::SIMD:
            return FixedVectorType::get(GenerateType(type->subtype.get()), Evaluate(type->capacity.get()));
        case TypeNodeType::TUPLE:
            throw std::runtime_error("Tuple types are not supported");
        case TypeNodeType::BOOL:
            return Type::getInt1Ty(*context_);
        case TypeNodeType::I8:
            return Type::getInt8Ty(*context_);
        case TypeNodeType::I16:
            return Type::getInt16Ty(*context_);
        case TypeNodeType::I32:
            return Type::getInt32Ty(*context_);
        case TypeNodeType::I64:
            return Type::getInt64Ty(*context_);
        case TypeNodeType::U8:
            return Type::getInt8Ty(*context_);
        case TypeNodeType::U16:
            return Type::getInt16Ty(*context_);
        case TypeNodeType::U32:
            return Type::getInt32Ty(*context_);
        case TypeNodeType::U64:
            return Type::getInt64Ty(*context_);
        case TypeNodeType::F32:
            return Type::getFloatTy(*context_);
        case TypeNodeType::F64:
            return Type::getDoubleTy(*context_);
        case TypeNodeType::TYPE_COUNT:
            throw std::runtime_error("Type count is invalid");
        case TypeNodeType::INFERED:
            throw std::runtime_error("Type is not specified");
    }
}

Value *LLVMBackend::GenerateRValue(AstNode *get) {
    if (auto compound = is<CompoundStatement>(get)) {
        scope_.PushScope();
        Value* last = nullptr;
        for (const auto &statement: compound->statements) {
            last = GenerateRValue(statement.get());
        }
        scope_.PopScope();
        return last;
    }
    if (auto variable = is<VariableNode>(get)) {
        auto* type = GenerateType(variable->type.get());
        auto* var = builder_->CreateAlloca(type, nullptr, variable->name);
        builder_->CreateStore(variable->value ? GenerateRValue(variable->value.get()) : ConstantAggregateZero::get(type), var);
        scope_.Declare(variable->name, var, variable->type.get());
        return var;
    }
    if (auto return_statement = is<ReturnNode>(get)) {
        if (return_statement->value == nullptr) {
            builder_->CreateRetVoid();
            return nullptr;
        }
        builder_->CreateRet(GenerateRValue(return_statement->value.get()));
        return nullptr;
    }
    if (const auto integer = is<IntegerNode>(get)) {
        return ConstantInt::get(*context_, APInt(64, integer->value, false));
    }
    if (const auto floating = is<FloatNode>(get)) {
        return ConstantFP::get(*context_, APFloat(floating->value));
    }
    if (const auto variable = is<IdentifierNode>(get)) {
        auto var = scope_.Lookup(variable->identifier);
        auto type = scope_.Type(variable->identifier);
        return builder_->CreateLoad(GenerateType(type), var);
    }
    if (const auto assign = is<AssignNode>(get)) {
        auto target = GenerateLValue(assign->target.get());
        auto value = GenerateRValue(assign->value.get());
        builder_->CreateStore(value, target);
        return value;
    }
    if (const auto binary = is<BinaryNode>(get)) {
        auto left = GenerateRValue(binary->left.get());
        auto right = GenerateRValue(binary->right.get());
        if (left->getType()->isFloatTy() || right->getType()->isFloatTy()) {
            throw std::runtime_error("Floats are not supported");
        }
        // integer math
        switch (binary->type) {
            case BinaryNodeType::ADD:
                return builder_->CreateAdd(left, right, "addtmp");
            case BinaryNodeType::SUBTRACT:
                return builder_->CreateSub(left, right, "subtmp");
            case BinaryNodeType::MULTIPLY:
                return builder_->CreateMul(left, right, "multmp");
            case BinaryNodeType::DIVIDE:
                return builder_->CreateSDiv(left, right, "divtmp");
            case BinaryNodeType::EXPONENT:
            case BinaryNodeType::MODULO:
                throw std::runtime_error("Unsupported complex binary expression type");
            case BinaryNodeType::BITWISE_OR:
                return builder_->CreateOr(left, right, "bitortmp");
            case BinaryNodeType::BITWISE_XOR:
                return builder_->CreateXor(left, right, "bitxortmp");
            case BinaryNodeType::BITWISE_AND:
                return builder_->CreateAnd(left, right, "bitandtmp");
            case BinaryNodeType::BITWISE_LEFT:
                return builder_->CreateShl(left, right, "shltmp");
            case BinaryNodeType::BITWISE_RIGHT:
                return builder_->CreateLShr(left, right, "shrtmp");
            case BinaryNodeType::GREATER:
                return builder_->CreateICmpSGT(left, right, "cmptmp");
            case BinaryNodeType::GREATER_EQUAL:
                return builder_->CreateICmpSGE(left, right, "cmptmp");
            case BinaryNodeType::LESS:
                return builder_->CreateICmpSLT(left, right, "cmptmp");
            case BinaryNodeType::LESS_EQUAL:
                return builder_->CreateICmpSLE(left, right, "cmptmp");
            case BinaryNodeType::EQUAL:
                return builder_->CreateICmpEQ(left, right, "cmptmp");
            case BinaryNodeType::NOT_EQUAL:
                return builder_->CreateICmpNE(left, right, "cmptmp");
            case BinaryNodeType::AND:
                return builder_->CreateLogicalAnd(left, right, "landtmp");
            case BinaryNodeType::OR:
                return builder_->CreateLogicalOr(left, right, "lortmp");
            case BinaryNodeType::INDEX:
            default:
                throw std::runtime_error("Unsupported binary expression type");
        }
    }
    throw std::runtime_error("Unsupported expression type");
}

Value * LLVMBackend::GenerateLValue(AstNode *get) {
    if (const auto variable = is<IdentifierNode>(get)) {
        auto var = scope_.Lookup(variable->identifier);
        auto type = scope_.Type(variable->identifier);
        return var;
    }
    throw std::runtime_error("Unsupported expression type");
}

void LLVMBackend::GenerateFunction(FunctionNode *function) {
    std::cout << "Function: " << function->name << std::endl;
    Type *return_type = GenerateType(function->return_type.get());
    std::vector<Type *> args;
    std::vector<std::string> names;
    for (const auto &arg: function->args) {
        if (const auto variable = is<VariableNode>(arg.get())) {
            auto type = GenerateType(variable->type.get());
            args.push_back(type);
            names.push_back(variable->name);
        } else {
            throw std::runtime_error("Unsupported argument type");
        }
    }

    auto function_type = FunctionType::get(return_type, args, false);

    auto func = Function::Create(function_type, GlobalValue::ExternalLinkage, function->name, module_.get());

    unsigned idx = 0;
    for (auto &arg: func->args()) {
        arg.setName(names[idx++]);
    }

    block_ = BasicBlock::Create(*context_, "entry", func);
    builder_->SetInsertPoint(block_);

    GenerateRValue(function->body.get());
}
