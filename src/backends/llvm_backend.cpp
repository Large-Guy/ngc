#include "llvm_backend.h"

#include <iostream>

#include "../memory_utils.h"
#include "../ast/nodes/AddressNode.h"
#include "../ast/nodes/assign_node.h"
#include "../ast/nodes/binary_node.h"
#include "../ast/nodes/cast_node.h"
#include "../ast/nodes/compound_statement.h"
#include "../ast/nodes/float_node.h"
#include "../ast/nodes/function_node.h"
#include "../ast/nodes/identifier_node.h"
#include "../ast/nodes/if_node.h"
#include "../ast/nodes/integer_node.h"
#include "../ast/nodes/literal_node.h"
#include "../ast/nodes/module_node.h"
#include "../ast/nodes/return_node.h"
#include "../ast/nodes/unary_node.h"
#include "../ast/nodes/variable_node.h"
#include "../ast/nodes/while_node.h"

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
    throw std::runtime_error("Type is not supported");
}

std::pair<Value *, std::unique_ptr<TypeNode>> LLVMBackend::Drill(std::pair<Value *, std::unique_ptr<TypeNode>> value) {
    if (value.second->subtype != nullptr && (value.second->subtype->type == TypeNodeType::BORROW || value.second->subtype->type == TypeNodeType::OWNER)) {
        auto new_type = std::move(value.second->subtype);
        return Drill(std::pair(builder_->CreateLoad(GenerateType(new_type.get()), value.first), std::move(new_type)));
    }
    return value;
}

std::pair<Value *, std::unique_ptr<TypeNode>> LLVMBackend::Cast(std::pair<Value *, std::unique_ptr<TypeNode>> value,
    TypeNode *type) {
    auto llvm_type = GenerateType(type);
    auto unique_type = UniqueCast<TypeNode>(type->Clone());

    if (value.second->Integer() && type->Integer()) {
        bool narrowing = value.second->Size() > type->Size();
        if (narrowing) {
            return {builder_->CreateTrunc(value.first, llvm_type), std::move(unique_type)};
        }
        if (value.second->Signed()) {
            return {builder_->CreateSExt(value.first, llvm_type), std::move(unique_type)};
        }
        return {builder_->CreateZExt(value.first, llvm_type), std::move(unique_type)};
    }

    if (value.second->Float() && type->Float()) {
        return {builder_->CreateFPCast(value.first, llvm_type), std::move(unique_type)};
    }

    if (value.second->Integer() && type->Float()) {
        if (value.second->Signed()) {
            return {builder_->CreateSIToFP(value.first, llvm_type), std::move(unique_type)};
        }
        return {builder_->CreateUIToFP(value.first, llvm_type), std::move(unique_type)};
    }
    if (value.second->Float() && type->Integer()) {
        if (type->Signed()) {
            return {builder_->CreateFPToSI(value.first, llvm_type), std::move(unique_type)};
        }
        return {builder_->CreateFPToUI(value.first, llvm_type), std::move(unique_type)};
    }

    if (value.second->Pointer() && type->Boolean()) {
        return {builder_->CreateIsNotNull(value.first), std::move(unique_type)};
    }

    throw std::runtime_error("Invalid cast");
}

std::pair<Value *, std::unique_ptr<TypeNode>> LLVMBackend::GenerateRValue(AstNode *get) {
    if (auto compound = is<CompoundStatement>(get)) {
        scope_.PushScope();
        std::pair<Value*, std::unique_ptr<TypeNode>> last = {};
        for (const auto &statement: compound->statements) {
            last = GenerateRValue(statement.get());
        }
        scope_.PopScope();
        return last;
    }
    if (auto variable = is<VariableNode>(get)) {
        auto* type = GenerateType(variable->type.get());
        auto* var = builder_->CreateAlloca(type, nullptr, variable->name);
        builder_->CreateStore(variable->value ? GenerateRValue(variable->value.get()).first : ConstantAggregateZero::get(type), var);
        scope_.Declare(variable->name, var, UniqueCast<TypeNode>(variable->type->Clone()));
        return std::pair(var, UniqueCast<TypeNode>(variable->type->Clone()));
    }
    if (auto address = is<AddressNode>(get)) {
        return GenerateLValue(get);
    }
    if (auto return_statement = is<ReturnNode>(get)) {
        if (return_statement->value == nullptr) {
            builder_->CreateRetVoid();
            return {};
        }
        builder_->CreateRet(GenerateRValue(return_statement->value.get()).first);
        return {};
    }
    if (auto if_statement = is<IfNode>(get)) {
        auto condition = GenerateRValue(if_statement->condition.get());
        auto then_block = BasicBlock::Create(*context_, "if.then", func);
        auto else_block = BasicBlock::Create(*context_, "if.else", func);
        auto merge_block = BasicBlock::Create(*context_, "if.merge", func);

        builder_->CreateCondBr(condition.first, then_block, else_block);

        builder_->SetInsertPoint(then_block);
        GenerateRValue(if_statement->then.get());
        builder_->CreateBr(merge_block);

        builder_->SetInsertPoint(else_block);
        if (if_statement->otherwise != nullptr) {
            GenerateRValue(if_statement->otherwise.get());
        }
        builder_->CreateBr(merge_block);

        builder_->SetInsertPoint(merge_block);
        return {};
    }
    if (auto while_statement = is<WhileNode>(get)) {
        auto cond_block = BasicBlock::Create(*context_, "while.cond", func);
        auto loop_block = BasicBlock::Create(*context_, "while.loop", func);
        auto merge_block = BasicBlock::Create(*context_, "while.merge", func);

        builder_->CreateBr(cond_block);
        builder_->SetInsertPoint(cond_block);
        auto condition = GenerateRValue(while_statement->condition.get());
        builder_->CreateCondBr(condition.first, loop_block, merge_block);

        builder_->SetInsertPoint(loop_block);
        auto result = GenerateRValue(while_statement->body.get());
        builder_->CreateBr(cond_block);

        builder_->SetInsertPoint(merge_block);
        return {};
    }
    if (const auto integer = is<IntegerNode>(get)) {
        return std::pair(ConstantInt::get(*context_, APInt(64, integer->value, false)), std::make_unique<TypeNode>(TypeNodeType::I64));
    }
    if (const auto floating = is<FloatNode>(get)) {
        return std::pair(ConstantFP::get(*context_, APFloat(floating->value)), std::make_unique<TypeNode>(TypeNodeType::F64));
    }
    if (const auto literal = is<LiteralNode>(get)) {
        switch (literal->type) {
            case LiteralNodeType::TRUE:
                return std::pair(ConstantInt::get(*context_, APInt(1, 1)), std::make_unique<TypeNode>(TypeNodeType::BOOL));
            case LiteralNodeType::FALSE:
                return std::pair(ConstantInt::get(*context_, APInt(1, 0)), std::make_unique<TypeNode>(TypeNodeType::BOOL));
            case LiteralNodeType::_NULL:
                return std::pair(ConstantPointerNull::get(PointerType::get(*context_, 0)), std::make_unique<TypeNode>(TypeNodeType::BORROW));
        }
    }
    if (const auto variable = is<IdentifierNode>(get)) {
        auto var = scope_.Lookup(variable->identifier);
        auto type = scope_.Type(variable->identifier);
        return std::pair(builder_->CreateLoad(GenerateType(type), var), UniqueCast<TypeNode>(type->Clone()));
    }
    if (const auto assign = is<AssignNode>(get)) {
        auto target = GenerateLValue(assign->target.get());
        target = Drill(std::move(target));
        auto value = GenerateRValue(assign->value.get());
        builder_->CreateStore(value.first, target.first);
        return value;
    }
    if (const auto binary = is<BinaryNode>(get)) {
        auto left = GenerateRValue(binary->left.get());
        auto right = GenerateRValue(binary->right.get());
        if (left.first->getType()->isFloatTy() || right.first->getType()->isFloatTy()) {
            throw std::runtime_error("Floats are not supported");
        }

        bool is_signed = left.second->Signed() || right.second->Signed();
        // integer math
        switch (binary->type) {
            case BinaryNodeType::ADD:
                return std::pair(builder_->CreateAdd(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::SUBTRACT:
                return std::pair(builder_->CreateSub(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::MULTIPLY:
                return std::pair(builder_->CreateMul(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::DIVIDE: {
                if (is_signed)
                    return std::pair(builder_->CreateSDiv(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
                return std::pair(builder_->CreateUDiv(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            }
            case BinaryNodeType::EXPONENT:
                throw std::runtime_error("Unsupported complex binary expression type");
            case BinaryNodeType::MODULO: {
                if (is_signed)
                    return std::pair(builder_->CreateSRem(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
                return std::pair(builder_->CreateURem(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            }
            case BinaryNodeType::BITWISE_OR:
                return std::pair(builder_->CreateOr(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::BITWISE_XOR:
                return std::pair(builder_->CreateXor(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::BITWISE_AND:
                return std::pair(builder_->CreateAnd(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::BITWISE_LEFT:
                return std::pair(builder_->CreateShl(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::BITWISE_RIGHT:
                return std::pair(builder_->CreateLShr(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::GREATER:
                return std::pair(builder_->CreateICmpSGT(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::GREATER_EQUAL:
                return std::pair(builder_->CreateICmpSGE(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::LESS:
                return std::pair(builder_->CreateICmpSLT(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::LESS_EQUAL:
                return std::pair(builder_->CreateICmpSLE(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::EQUAL:
                return std::pair(builder_->CreateICmpEQ(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::NOT_EQUAL:
                return std::pair(builder_->CreateICmpNE(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::AND:
                return std::pair(builder_->CreateLogicalAnd(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::OR:
                return std::pair(builder_->CreateLogicalOr(left.first, right.first), std::make_unique<TypeNode>(TypeNodeType::I64));
            case BinaryNodeType::INDEX:
            default:
                throw std::runtime_error("Unsupported binary expression type");
        }
    }
    if (const auto unary = is<UnaryNode>(get)) {
        auto x = GenerateRValue(unary->operand.get());
        if (x.first->getType()->isFloatTy()) {
            throw std::runtime_error("Floats are not supported");
        }

        switch (unary->type) {
            case UnaryNodeType::NEGATE:
                return std::pair(builder_->CreateNeg(x.first, "negtmp"), std::make_unique<TypeNode>(TypeNodeType::I64));
            case UnaryNodeType::NOT:
                return std::pair(builder_->CreateNot(x.first, "nottmp"), std::make_unique<TypeNode>(TypeNodeType::I64));
            default:
                throw std::runtime_error("Unsupported unary expression type");
        }
    }
    if (const auto cast = is<CastNode>(get)) {
        if (cast->type == CastNodeType::STATIC) {
            return Cast(GenerateRValue(cast->expression.get()), cast->target.get());
        }
        auto x = GenerateRValue(cast->expression.get());
        auto type = GenerateType(cast->target.get());
        return {builder_->CreateBitCast(x.first, type), UniqueCast<TypeNode>(cast->target->Clone())};
    }
    throw std::runtime_error("Unsupported expression type");
}

std::pair<Value *, std::unique_ptr<TypeNode>> LLVMBackend::GenerateLValue(AstNode *get) {
    if (const auto variable = is<IdentifierNode>(get)) {
        auto var = scope_.Lookup(variable->identifier);
        auto type = scope_.Type(variable->identifier);
        return std::pair(var, UniqueCast<TypeNode>(type->Clone()));
    }
    if (const auto address = is<AddressNode>(get)) {
        return GenerateLValue(address->target.get());
    }
    throw std::runtime_error("Unsupported expression type");
}

void LLVMBackend::GenerateFunction(FunctionNode *function) {
    std::cout << "Function: " << function->name << std::endl;
    Type *return_type = GenerateType(function->return_type.get());
    std::vector<Type *> args;
    std::vector<std::string> names;
    std::vector<std::unique_ptr<TypeNode>> types;
    for (const auto &arg: function->args) {
        if (const auto variable = is<VariableNode>(arg.get())) {
            auto type = GenerateType(variable->type.get());
            args.push_back(type);
            names.push_back(variable->name);
            types.push_back(UniqueCast<TypeNode>(variable->type->Clone()));
        } else {
            throw std::runtime_error("Unsupported argument type");
        }
    }

    auto function_type = FunctionType::get(return_type, args, false);

    func = Function::Create(function_type, GlobalValue::ExternalLinkage, function->name, module_.get());

    scope_.PushScope();
    unsigned idx = 0;
    for (auto &arg: func->args()) {
        arg.setName(names[idx]);
        scope_.Declare(names[idx], &arg, std::move(types[idx]));
        idx++;
    }

    auto block = BasicBlock::Create(*context_, "entry", func);
    builder_->SetInsertPoint(block);

    GenerateRValue(function->body.get());

    func = nullptr;

    scope_.PopScope();
}
