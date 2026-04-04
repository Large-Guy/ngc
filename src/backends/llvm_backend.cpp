#include "llvm_backend.h"

#include <iostream>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>

#include "../memory_utils.h"
#include "../ast/nodes/AddressNode.h"
#include "../ast/nodes/assign_node.h"
#include "../ast/nodes/binary_node.h"
#include "../ast/nodes/call_node.h"
#include "../ast/nodes/cast_node.h"
#include "../ast/nodes/compound_statement.h"
#include "../ast/nodes/float_node.h"
#include "../ast/nodes/function_node.h"
#include "../ast/nodes/heap_node.h"
#include "../ast/nodes/identifier_node.h"
#include "../ast/nodes/if_node.h"
#include "../ast/nodes/integer_node.h"
#include "../ast/nodes/literal_node.h"
#include "../ast/nodes/module_node.h"
#include "../ast/nodes/return_node.h"
#include "../ast/nodes/tuple_node.h"
#include "../ast/nodes/unary_node.h"
#include "../ast/nodes/variable_node.h"
#include "../ast/nodes/while_node.h"

const static TypeNode BOOLEAN = TypeNode(TypeNodeType::BOOL);

void LLVMBackend::Generate(std::vector<std::unique_ptr<AstNode> > nodes) {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    std::string target_triple = sys::getDefaultTargetTriple();

    std::string error;
    auto* target = llvm::TargetRegistry::lookupTarget(target_triple, error);
    if (!target) {
        llvm::errs() << "Target lookup failed: " << error;
        return;
    }

    auto cpu = llvm::sys::getHostCPUName();

    llvm::TargetOptions options;
    auto target_machine = target->createTargetMachine(target_triple, cpu, "", options, Reloc::PIC_);

    context_ = std::make_unique<LLVMContext>();
    builder_ = std::make_unique<IRBuilder<> >(*context_);

    scope_.PushScope();

    for (const auto& node: nodes) {
        if (const auto module = is<ModuleNode>(node.get())) {
            module_ = std::make_unique<Module>(module->path, *context_);
            module_->setTargetTriple(target_triple);
            module_->setDataLayout(target_machine->createDataLayout());
            std::cout << "Module: " << module->path << std::endl;

            for (const auto& statement: module->statements) {
                if (const auto function = is<FunctionNode>(statement.get())) {
                    GeneratePrototype(function);
                } else if (const auto variable = is<VariableNode>(statement.get())) {
                    GenerateVariable(variable);
                }
            }

            for (const auto& statement: module->statements) {
                if (const auto function = is<FunctionNode>(statement.get())) {
                    GenerateFunction(function);
                }
            }
        }
    }

    scope_.PopScope();

    if (!module_) {
        throw std::runtime_error("No module node was provided");
    }

    if (verifyModule(*module_, &llvm::errs())) {
        llvm::errs() << "Module verification failed; skipping optimization and codegen.\n";
        module_->print(llvm::outs(), nullptr);
        return;
    }

    module_->print(llvm::outs(), nullptr);

    PassBuilder pass_builder(target_machine);
    LoopAnalysisManager lam;
    FunctionAnalysisManager fam;
    CGSCCAnalysisManager cgam;
    ModuleAnalysisManager mam;

    pass_builder.registerModuleAnalyses(mam);
    pass_builder.registerCGSCCAnalyses(cgam);
    pass_builder.registerFunctionAnalyses(fam);
    pass_builder.registerLoopAnalyses(lam);
    pass_builder.crossRegisterProxies(lam, fam, cgam, mam);

    ModulePassManager pass_manager = pass_builder.buildPerModuleDefaultPipeline(OptimizationLevel::O1);
    pass_manager.run(*module_, mam);

    llvm::outs() << "\n=== AFTER O2 ===\n";
    module_->print(llvm::outs(), nullptr);

    std::error_code ec;
    raw_fd_ostream dest("output.o", ec, sys::fs::OpenFlags::OF_None);
    if (ec) {
        llvm::errs() << "Could not open output file: " << ec.message() << "\n";
        return;
    }

    legacy::PassManager codegen_pass_manager;
    target_machine->addPassesToEmitFile(codegen_pass_manager, dest, nullptr, CodeGenFileType::ObjectFile);
    codegen_pass_manager.run(*module_);
    dest.close();
}

int64_t LLVMBackend::Evaluate(ExpressionNode* unique) {
    throw std::runtime_error("Not implemented");
}

std::unique_ptr<TypeNode> LLVMBackend::EvaluateType() {
}

Type* LLVMBackend::GenerateType(const TypeNode* type) {
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
        case TypeNodeType::FUNCTION:
            return PointerType::get(*context_, 0);
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
    }
    throw std::runtime_error("Type is not supported");
}

std::pair<Value*, std::unique_ptr<TypeNode> > LLVMBackend::Drill(std::pair<Value*, std::unique_ptr<TypeNode> > value,
                                                                 const TypeNode* expected) {
    if (expected != nullptr && value.second->Equal(expected, true)) {
        return value;
    }
    if (value.second != nullptr && (value.second->type == TypeNodeType::BORROW || value.second->type ==
                                    TypeNodeType::OWNER)) {
        auto new_type = std::move(value.second->subtype);
        return Drill(std::pair(builder_->CreateLoad(GenerateType(new_type.get()), value.first), std::move(new_type)),
                     expected);
    }
    return value;
}

std::pair<Value*, std::unique_ptr<TypeNode> > LLVMBackend::Cast(std::pair<Value*, std::unique_ptr<TypeNode> > value,
                                                                const TypeNode* type) {
    if (type == nullptr || value.second->Equal(type, true)) {
        // null type implies no cast
        return {value.first, UniqueCast<TypeNode>(type->Clone())};
    }
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

    if (value.second->Pointer() && type->Pointer()) {
        if (value.second->type == TypeNodeType::BORROW && type->type == TypeNodeType::OWNER) {
            throw std::runtime_error("Cannot cast borrow to owner");
        }
        if (value.second->subtype->Equal(type->subtype.get(), true)) {
            return {value.first, UniqueCast<TypeNode>(type->Clone())};
        }
    }

    throw std::runtime_error("Invalid cast");
}

std::unique_ptr<TypeNode> LLVMBackend::Promote(std::pair<Value*, std::unique_ptr<TypeNode> >& a,
                                               std::pair<Value*, std::unique_ptr<TypeNode> >& b) {
    if (a.second->Integer() && b.second->Integer()) {
        //promote to larger type
        if (a.second->Size() > b.second->Size()) {
            return UniqueCast<TypeNode>(a.second->Clone());
        }
        return UniqueCast<TypeNode>(b.second->Clone());
    }
    if (a.second->Float() && b.second->Float()) {
        //promote to larger type
        if (a.second->Size() > b.second->Size()) {
            return UniqueCast<TypeNode>(a.second->Clone());
        }
        return UniqueCast<TypeNode>(b.second->Clone());
    }
    if (a.second->Float() && b.second->Integer()) {
        return UniqueCast<TypeNode>(a.second->Clone()); // promote to float
    }
    if (b.second->Float() && a.second->Integer()) {
        return UniqueCast<TypeNode>(b.second->Clone()); // promote to float
    }
    std::cerr << "Unhandled promotion rule, could be possible source of bug" << std::endl;
    return UniqueCast<TypeNode>(a.second->Clone());
}

std::pair<Value*, std::unique_ptr<TypeNode> > LLVMBackend::GenerateRValue(AstNode* get, const TypeNode* expected) {
    if (auto compound = is<CompoundStatement>(get)) {
        scope_.PushScope();
        std::pair<Value*, std::unique_ptr<TypeNode> > last = {};
        for (const auto& statement: compound->statements) {
            last = GenerateRValue(statement.get(), expected);

            if (builder_->GetInsertBlock()->getTerminator())
                break;
        }
        scope_.PopScope();
        if (last.first == nullptr)
            return {};
        if (expected == nullptr)
            return last;
        if (expected->type == TypeNodeType::VOID)
            return {};
        return Cast(std::move(last), expected);
    }
    if (auto variable = is<VariableNode>(get)) {
        if (variable->type == nullptr) {
            //inferred
            if (variable->value == nullptr)
                throw std::runtime_error("Inferred variable value is null");
            auto val = GenerateRValue(variable->value.get(), nullptr);

            auto* type = GenerateType(val.second.get());
            auto* var = builder_->CreateAlloca(type, nullptr, variable->name);
            builder_->CreateStore(val.first, var);
            auto ptr_type = std::make_unique<TypeNode>(TypeNodeType::OWNER,
                                                       UniqueCast<TypeNode>(val.second->Clone()));
            scope_.Declare(variable->name, var, UniqueCast<TypeNode>(ptr_type->Clone()));
            return std::pair(var, UniqueCast<TypeNode>(ptr_type->Clone()));
        }
        auto* type = GenerateType(variable->type.get());
        auto* var = builder_->CreateAlloca(type, nullptr, variable->name);
        builder_->CreateStore(variable->value
                                  ? GenerateRValue(variable->value.get(), variable->type.get()).first
                                  : Constant::getNullValue(type), var);
        auto ptr_type = std::make_unique<TypeNode>(TypeNodeType::OWNER, UniqueCast<TypeNode>(variable->type->Clone()));
        scope_.Declare(variable->name, var, UniqueCast<TypeNode>(ptr_type->Clone()));
        return std::pair(var, UniqueCast<TypeNode>(ptr_type->Clone()));
    }
    if (auto address = is<AddressNode>(get)) {
        return Cast(GenerateLValue(get), expected);
    }
    if (auto return_statement = is<ReturnNode>(get)) {
        if (return_statement->value == nullptr) {
            builder_->CreateBr(exit);
            return {};
        }
        auto result = GenerateRValue(return_statement->value.get(), expected);
        //result.first
        builder_->CreateStore(result.first, ret, false);
        builder_->CreateBr(exit);
        return {};
    }
    if (auto if_statement = is<IfNode>(get)) {
        auto condition = GenerateRValue(if_statement->condition.get(), &BOOLEAN);
        auto then_block = BasicBlock::Create(*context_, "if.then", func);
        auto else_block = BasicBlock::Create(*context_, "if.else", func);
        auto merge_block = BasicBlock::Create(*context_, "if.merge", func);

        builder_->CreateCondBr(condition.first, then_block, else_block);

        builder_->SetInsertPoint(then_block);
        GenerateRValue(if_statement->then.get(), expected);
        if (!then_block->getTerminator())
            builder_->CreateBr(merge_block);

        builder_->SetInsertPoint(else_block);
        if (if_statement->otherwise != nullptr) {
            GenerateRValue(if_statement->otherwise.get(), expected);
        }

        if (!else_block->getTerminator())
            builder_->CreateBr(merge_block);

        if (merge_block->hasNPredecessors(0)) {
            merge_block->eraseFromParent();
        } else {
            builder_->SetInsertPoint(merge_block);
        }
        return {};
    }
    if (auto while_statement = is<WhileNode>(get)) {
        auto cond_block = BasicBlock::Create(*context_, "while.cond", func);
        auto loop_block = BasicBlock::Create(*context_, "while.loop", func);
        auto merge_block = BasicBlock::Create(*context_, "while.merge", func);

        builder_->CreateBr(cond_block);
        builder_->SetInsertPoint(cond_block);
        auto condition = GenerateRValue(while_statement->condition.get(), &BOOLEAN);
        builder_->CreateCondBr(condition.first, loop_block, merge_block);

        builder_->SetInsertPoint(loop_block);
        auto result = GenerateRValue(while_statement->body.get(), nullptr);
        builder_->CreateBr(cond_block);

        builder_->SetInsertPoint(merge_block);
        return {};
    }
    if (const auto integer = is<IntegerNode>(get)) {
        Value* val = nullptr;
        if (expected != nullptr && expected->Integer()) {
            switch (expected->type) {
                case TypeNodeType::I8:
                    val = ConstantInt::get(*context_, APInt(8, integer->value));
                    break;
                case TypeNodeType::I16:
                    val = ConstantInt::get(*context_, APInt(16, integer->value));
                    break;
                case TypeNodeType::I32:
                    val = ConstantInt::get(*context_, APInt(32, integer->value));
                    break;
                case TypeNodeType::I64:
                    val = ConstantInt::get(*context_, APInt(64, integer->value));
                    break;
                default:
                    throw std::runtime_error("Unsupported integer type");
            }
            return {val, UniqueCast<TypeNode>(expected->Clone())};
        }
        auto value = std::make_pair(ConstantInt::get(*context_, APInt(32, integer->value)),
                                    std::make_unique<TypeNode>(TypeNodeType::I32));
        if (expected == nullptr)
            return value;
        return Cast(std::move(value), expected);
    }
    if (const auto floating = is<FloatNode>(get)) {
        if (expected == nullptr) {
            if (floating->is_double)
                return std::pair(ConstantFP::get(*context_, APFloat(floating->value)),
                                 std::make_unique<TypeNode>(TypeNodeType::F64));
            return std::pair(ConstantFP::get(*context_, APFloat(static_cast<float>(floating->value))),
                             std::make_unique<TypeNode>(TypeNodeType::F32));
        }
        return Cast(std::pair(ConstantFP::get(*context_, APFloat(floating->value)),
                              std::make_unique<TypeNode>(TypeNodeType::F64)), expected);
    }
    if (const auto tuple = is<TupleNode>(get)) {
        auto totalSize = 0;
        for (auto& node: tuple->elements) {
            auto type = EvaluateType();
        }
        auto arr = builder_->CreateAlloca();
    }
    if (const auto literal = is<LiteralNode>(get)) {
        switch (literal->type) {
            case LiteralNodeType::TRUE:
                return std::pair(ConstantInt::get(*context_, APInt(1, 1)),
                                 std::make_unique<TypeNode>(TypeNodeType::BOOL));
            case LiteralNodeType::FALSE:
                return std::pair(ConstantInt::get(*context_, APInt(1, 0)),
                                 std::make_unique<TypeNode>(TypeNodeType::BOOL));
            case LiteralNodeType::_NULL:
                return std::pair(ConstantPointerNull::get(PointerType::get(*context_, 0)),
                                 std::make_unique<TypeNode>(TypeNodeType::BORROW));
        }
    }
    if (const auto identifier = is<IdentifierNode>(get)) {
        auto var = scope_.Lookup(identifier->identifier);
        auto type = scope_.Type(identifier->identifier);
        auto drill = Drill({var, UniqueCast<TypeNode>(type->Clone())}, expected);
        return drill;
    }
    if (const auto assign = is<AssignNode>(get)) {
        auto target = GenerateLValue(assign->target.get());
        auto type_target = target.second.get();
        while (type_target->subtype->Pointer())
            type_target = type_target->subtype.get();
        target = Drill(std::move(target), type_target);
        auto value = GenerateRValue(assign->value.get(), target.second->subtype.get());
        builder_->CreateStore(value.first, target.first);
        return value;
    }
    if (const auto binary = is<BinaryNode>(get)) {
        auto left = GenerateRValue(binary->left.get(), nullptr);
        auto right = GenerateRValue(binary->right.get(), nullptr);
        if (left.first->getType()->isFloatTy() || right.first->getType()->isFloatTy()) {
            auto promotion = Promote(left, right);
            left = Cast(std::move(left), promotion.get());
            right = Cast(std::move(right), promotion.get());

            switch (binary->type) {
                case BinaryNodeType::ADD:
                    return std::pair(builder_->CreateFAdd(left.first, right.first),
                                     std::move(promotion));
                case BinaryNodeType::SUBTRACT:
                    return std::pair(builder_->CreateFSub(left.first, right.first),
                                     std::move(promotion));
                case BinaryNodeType::MULTIPLY:
                    return std::pair(builder_->CreateFMul(left.first, right.first),
                                     std::move(promotion));
                case BinaryNodeType::DIVIDE: {
                    return std::pair(builder_->CreateFDiv(left.first, right.first),
                                     std::move(promotion));
                }
                case BinaryNodeType::EXPONENT:
                    return std::pair(
                        builder_->CreateIntrinsic(Intrinsic::pow, left.first->getType(),
                                                  {left.first, right.first}),
                        std::move(promotion)
                    );
                case BinaryNodeType::MODULO: {
                    return std::pair(builder_->CreateFRem(left.first, right.first),
                                     std::move(promotion));
                }
                case BinaryNodeType::BITWISE_OR:
                    return std::pair(builder_->CreateOr(left.first, right.first),
                                     std::make_unique<TypeNode>(TypeNodeType::I64));
                case BinaryNodeType::BITWISE_XOR:
                    return std::pair(builder_->CreateXor(left.first, right.first),
                                     std::make_unique<TypeNode>(TypeNodeType::I64));
                case BinaryNodeType::BITWISE_AND:
                    return std::pair(builder_->CreateAnd(left.first, right.first),
                                     std::make_unique<TypeNode>(TypeNodeType::I64));
                case BinaryNodeType::BITWISE_LEFT:
                    return std::pair(builder_->CreateShl(left.first, right.first),
                                     std::make_unique<TypeNode>(TypeNodeType::I64));
                case BinaryNodeType::BITWISE_RIGHT:
                    return std::pair(builder_->CreateLShr(left.first, right.first),
                                     std::make_unique<TypeNode>(TypeNodeType::I64));
                case BinaryNodeType::GREATER:
                    return std::pair(builder_->CreateFCmpOGT(left.first, right.first),
                                     std::make_unique<TypeNode>(TypeNodeType::BOOL));
                case BinaryNodeType::GREATER_EQUAL:
                    return std::pair(builder_->CreateFCmpOGE(left.first, right.first),
                                     std::make_unique<TypeNode>(TypeNodeType::BOOL));
                case BinaryNodeType::LESS:
                    return std::pair(builder_->CreateFCmpOLT(left.first, right.first),
                                     std::make_unique<TypeNode>(TypeNodeType::BOOL));
                case BinaryNodeType::LESS_EQUAL:
                    return std::pair(builder_->CreateFCmpOLE(left.first, right.first),
                                     std::make_unique<TypeNode>(TypeNodeType::BOOL));
                case BinaryNodeType::EQUAL:
                    return std::pair(builder_->CreateFCmpOEQ(left.first, right.first),
                                     std::make_unique<TypeNode>(TypeNodeType::BOOL));
                case BinaryNodeType::NOT_EQUAL:
                    return std::pair(builder_->CreateFCmpONE(left.first, right.first),
                                     std::make_unique<TypeNode>(TypeNodeType::BOOL));
                case BinaryNodeType::INDEX:
                default:
                    throw std::runtime_error("Unsupported binary expression type");
            }
        }

        auto promotion = Promote(left, right);
        left = Cast(std::move(left), promotion.get());
        right = Cast(std::move(right), promotion.get());

        bool is_signed = left.second->Signed() || right.second->Signed();
        // integer math
        switch (binary->type) {
            case BinaryNodeType::ADD:
                return std::pair(builder_->CreateAdd(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::SUBTRACT:
                return std::pair(builder_->CreateSub(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::MULTIPLY:
                return std::pair(builder_->CreateMul(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::DIVIDE: {
                if (is_signed)
                    return std::pair(builder_->CreateSDiv(left.first, right.first),
                                     std::move(promotion));
                return std::pair(builder_->CreateUDiv(left.first, right.first),
                                 std::move(promotion));
            }
            case BinaryNodeType::EXPONENT:
                return std::pair(
                    builder_->CreateIntrinsic(Intrinsic::powi, left.first->getType(),
                                              {left.first, right.first}),
                    std::move(promotion)
                );
            case BinaryNodeType::MODULO: {
                if (is_signed)
                    return std::pair(builder_->CreateSRem(left.first, right.first),
                                     std::move(promotion));
                return std::pair(builder_->CreateURem(left.first, right.first),
                                 std::move(promotion));
            }
            case BinaryNodeType::BITWISE_OR:
                return std::pair(builder_->CreateOr(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::BITWISE_XOR:
                return std::pair(builder_->CreateXor(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::BITWISE_AND:
                return std::pair(builder_->CreateAnd(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::BITWISE_LEFT:
                return std::pair(builder_->CreateShl(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::BITWISE_RIGHT:
                return std::pair(builder_->CreateLShr(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::GREATER:
                return std::pair(builder_->CreateICmpSGT(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::GREATER_EQUAL:
                return std::pair(builder_->CreateICmpSGE(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::LESS:
                return std::pair(builder_->CreateICmpSLT(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::LESS_EQUAL:
                return std::pair(builder_->CreateICmpSLE(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::EQUAL:
                return std::pair(builder_->CreateICmpEQ(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::NOT_EQUAL:
                return std::pair(builder_->CreateICmpNE(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::AND:
                return std::pair(builder_->CreateLogicalAnd(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::OR:
                return std::pair(builder_->CreateLogicalOr(left.first, right.first),
                                 std::move(promotion));
            case BinaryNodeType::INDEX:
            default:
                throw std::runtime_error("Unsupported binary expression type");
        }
    }
    if (const auto unary = is<UnaryNode>(get)) {
        auto x = GenerateRValue(unary->operand.get(), nullptr);

        switch (unary->type) {
            case UnaryNodeType::NEGATE:
                if (x.second->Float()) {
                    return std::pair(builder_->CreateFNeg(x.first, "fnegtmp"), UniqueCast<TypeNode>(x.second->Clone()));
                }
                return std::pair(builder_->CreateNeg(x.first, "negtmp"), UniqueCast<TypeNode>(x.second->Clone()));
            case UnaryNodeType::NOT:
                x = Cast(std::move(x), &BOOLEAN);
                return std::pair(builder_->CreateNot(x.first, "nottmp"),
                                 std::make_unique<TypeNode>(TypeNodeType::BOOL));
            default:
                throw std::runtime_error("Unsupported unary expression type");
        }
    }
    if (const auto heap = is<HeapNode>(get)) {
        if (expected == nullptr)
            throw std::runtime_error("Heap nodes need expected types");
        auto x = GenerateRValue(heap->expression.get(), expected->subtype.get());
        auto type = GenerateType(x.second.get());
        auto ptr = builder_->CreateMalloc(Type::getInt64Ty(*context_), type,
                                          ConstantInt::get(*context_, APInt(64, x.second->Size())), nullptr);
        builder_->CreateStore(x.first, ptr);
        return {ptr, UniqueCast<TypeNode>(expected->Clone())};
    }
    if (const auto cast = is<CastNode>(get)) {
        if (cast->type == CastNodeType::STATIC) {
            return Cast(GenerateRValue(cast->expression.get(), nullptr), cast->target.get());
        }
        auto x = GenerateRValue(cast->expression.get(), nullptr);
        auto type = GenerateType(cast->target.get());
        return {builder_->CreateBitCast(x.first, type), UniqueCast<TypeNode>(cast->target->Clone())};
    }
    if (const auto call = is<CallNode>(get)) {
        auto callee = GenerateLValue(call->callable.get());
        auto function = dyn_cast<Function>(callee.first);
        std::vector<Value*> args;
        args.reserve(call->args.size());
        auto signature = signatures[function];
        for (auto i = 0; i < call->args.size(); i++) {
            const auto& arg = call->args[i];
            const auto& expect = signature->args[i]->type;
            args.push_back(GenerateRValue(arg.get(), expect.get()).first);
        }
        auto val = builder_->CreateCall(FunctionCallee(function->getFunctionType(), function), args);
        return {val, UniqueCast<TypeNode>(callee.second->subtype->Clone())};
    }
    throw std::runtime_error("Unsupported expression type");
}

std::pair<Value*, std::unique_ptr<TypeNode> > LLVMBackend::GenerateLValue(AstNode* get) {
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

void LLVMBackend::GeneratePrototype(FunctionNode* function) {
    std::cout << "Prototype Function: " << function->name << std::endl;
    Type* return_type = GenerateType(function->type.get());
    std::vector<Type*> args;
    std::vector<std::string> names;
    std::vector<std::unique_ptr<TypeNode> > types;
    for (const auto& arg: function->args) {
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

    auto func = Function::Create(function_type, GlobalValue::ExternalLinkage, function->name, module_.get());

    signatures[func] = function;

    auto type = std::make_unique<TypeNode>(TypeNodeType::FUNCTION);
    type->subtype = UniqueCast<TypeNode>(function->type->Clone());

    scope_.Declare(function->name, func, std::move(type));
}

void LLVMBackend::GenerateFunction(FunctionNode* function) {
    std::cout << "Function: " << function->name << std::endl;
    Type* return_type = GenerateType(function->type.get());
    std::vector<Type*> args;
    std::vector<std::string> names;
    std::vector<std::unique_ptr<TypeNode> > types;
    for (const auto& arg: function->args) {
        if (const auto variable = is<VariableNode>(arg.get())) {
            auto type = GenerateType(variable->type.get());
            args.push_back(type);
            names.push_back(variable->name);
            types.push_back(UniqueCast<TypeNode>(variable->type->Clone()));
        } else {
            throw std::runtime_error("Unsupported argument type");
        }
    }

    func = module_->getFunction(function->name);

    auto block = BasicBlock::Create(*context_, "entry", func);
    builder_->SetInsertPoint(block);

    if (function->type->type != TypeNodeType::VOID) {
        ret = builder_->CreateAlloca(return_type, nullptr, "return.addr");
        builder_->CreateStore(Constant::getNullValue(return_type), ret);
    }

    scope_.PushScope();
    unsigned idx = 0;
    for (auto& arg: func->args()) {
        arg.setName(names[idx]);
        auto* arg_slot = builder_->CreateAlloca(arg.getType(), nullptr, names[idx] + ".addr");
        builder_->CreateStore(&arg, arg_slot);
        auto ptr_arg = std::make_unique<TypeNode>(TypeNodeType::OWNER, UniqueCast<TypeNode>(types[idx]->Clone()));
        scope_.Declare(names[idx], arg_slot, std::move(ptr_arg));
        idx++;
    }

    exit = BasicBlock::Create(*context_, "exit");

    GenerateRValue(function->body.get(), function->type.get());

    if (!builder_->GetInsertBlock()->getTerminator())
        builder_->CreateBr(exit);

    exit->insertInto(func);

    builder_->SetInsertPoint(exit);

    if (function->type->type != TypeNodeType::VOID) {
        auto load = builder_->CreateLoad(return_type, ret);
        builder_->CreateRet(load);
    } else {
        builder_->CreateRetVoid();
    }

    func->print(llvm::outs());

    if (verifyFunction(*func, &llvm::errs())) {
        throw std::runtime_error("Function verification failed: " + function->name);
    }

    func = nullptr;

    scope_.PopScope();

    exit = nullptr;
}

void LLVMBackend::GenerateVariable(VariableNode* variable) {
    auto var = module_->getOrInsertGlobal(variable->name, GenerateType(variable->type.get()));
    auto gvar = llvm::cast<GlobalVariable>(var);
    gvar->setInitializer(Constant::getNullValue(GenerateType(variable->type.get())));
    auto ptr = std::make_unique<TypeNode>(TypeNodeType::OWNER, UniqueCast<TypeNode>(variable->type->Clone()));

    scope_.Declare(variable->name, gvar, std::move(ptr));
}
