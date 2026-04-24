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

#include "memory_utils.h"
#include "ast/nodes/AddressNode.h"
#include "ast/nodes/assign_node.h"
#include "ast/nodes/binary_node.h"
#include "ast/nodes/call_node.h"
#include "ast/nodes/cast_node.h"
#include "ast/nodes/compound_statement.h"
#include "ast/nodes/field_node.h"
#include "ast/nodes/float_node.h"
#include "ast/nodes/for_node.h"
#include "ast/nodes/function_node.h"
#include "ast/nodes/heap_node.h"
#include "ast/nodes/identifier_node.h"
#include "ast/nodes/if_node.h"
#include "ast/nodes/index_node.h"
#include "ast/nodes/integer_node.h"
#include "ast/nodes/literal_node.h"
#include "ast/nodes/module_node.h"
#include "ast/nodes/return_node.h"
#include "ast/nodes/struct_node.h"
#include "ast/nodes/tuple_node.h"
#include "ast/nodes/unary_node.h"
#include "ast/nodes/variable_node.h"
#include "ast/nodes/while_node.h"

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
            module_ = std::make_unique<Module>(module->name, *context_);
            module_->setTargetTriple(target_triple);
            module_->setDataLayout(target_machine->createDataLayout());
            std::cout << "Module: " << module->name << std::endl;

            for (const auto& statement: module->statements) {
                if (const auto function = is<FunctionNode>(statement.get())) {
                    GeneratePrototype(function);
                } else if (const auto variable = is<VariableNode>(statement.get())) {
                    GenerateVariable(variable);
                } else if (const auto structure = is<StructNode>(statement.get())) {
                    GenerateStructure(structure);
                }
            }

            for (const auto& statement: module->statements) {
                if (const auto function = is<FunctionNode>(statement.get())) {
                    GenerateFunction(function);
                }
            }
        }
    }

    scope_.PopScope(this, builder_.get(), current_block);

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

std::unique_ptr<TypeNode> LLVMBackend::EvaluateRType(AstNode* get) {
    if (auto identifier = is<IdentifierNode>(get)) {
    }
}

std::unique_ptr<TypeNode> LLVMBackend::EvaluateLType(AstNode* get) {
}

Type* LLVMBackend::GenerateType(const TypeNode* type, const std::string& name) {
    switch (type->type) {
        case TypeNodeType::VOID:
            return Type::getVoidTy(*context_);
        case TypeNodeType::STRUCT: {
            auto subtypes = std::vector<Type*>();
            for (const auto& subtype: type->subtype) {
                subtypes.push_back(GenerateType(subtype.get()));
            }
            StructType* structure = nullptr;
            if (name.empty()) {
                structure = StructType::get(*context_, subtypes); // declaration and definition
            } else {
                structure = StructType::getTypeByName(*context_, name); // definition
            }
            structure->setBody(subtypes);
            if (!name.empty())
                structure->setName(name);
            return structure;
        }
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
        case TypeNodeType::SIMD: {
            if (type->subtype[0]->Vectorizable()) {
                return FixedVectorType::get(GenerateType(type->subtype[0].get()), EvaluateInt(type->capacity.get()));
            }
            // default to a plain static array
            return ArrayType::get(GenerateType(type->subtype[0].get()), EvaluateInt(type->capacity.get()));
        }
        case TypeNodeType::TENSOR: {
            //Expect capacity to be a tuple
            auto tuple = dynamic_cast<TupleNode*>(type->capacity.get());
            if (!tuple)
                throw std::runtime_error("Expected tuple in matrix capacity");
            auto elem_count = 1;
            for (const auto& size : tuple->elements) {
                elem_count *= EvaluateInt(size.get());
            }
            return FixedVectorType::get(GenerateType(type->subtype[0].get()), elem_count);
        }
        case TypeNodeType::TUPLE: {
            auto subtypes = std::vector<Type*>();
            for (const auto& subtype: type->subtype) {
                subtypes.push_back(GenerateType(subtype.get()));
            }
            auto structure = StructType::get(*context_, subtypes);
            return structure;
        }
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
        auto new_type = std::move(value.second->subtype[0]);
        return Drill(std::pair(builder_->CreateLoad(GenerateType(new_type.get()), value.first), std::move(new_type)),
                     expected);
    }
    return value;
}

std::pair<Value*, std::unique_ptr<TypeNode> > LLVMBackend::Cast(std::pair<Value*, std::unique_ptr<TypeNode> > value,
                                                                const TypeNode* type, Type* override_type) {
    if (type == nullptr || value.second->Equal(type, true)) {
        // null type implies no cast
        return {value.first, UniqueCast<TypeNode>(type->Clone())};
    }
    Type* llvm_type = override_type;
    if (!llvm_type)
        llvm_type = GenerateType(type);
    auto unique_type = UniqueCast<TypeNode>(type->Clone());
    
    if (value.second->type == TypeNodeType::SIMD && type->type == TypeNodeType::SIMD) {
        auto val_cap = EvaluateInt(value.second->capacity.get());
        auto res_cap = EvaluateInt(type->capacity.get());

        //Convert element type
        auto val_elem_type = value.second->subtype[0].get();
        auto res_elem_type = type->subtype[0].get();

        std::vector<int> shuffle;
        shuffle.reserve(res_cap);
        for (auto i = 0; i < res_cap; i++) {
            shuffle.push_back(i < val_cap ? i : val_cap + (i - val_cap));
        }

        //build vector
        auto val = builder_->CreateShuffleVector(value.first,
                                                 Constant::getNullValue(GenerateType(value.second.get())),
                                                 shuffle);

        // cast subtype by lying about true type
        
        auto cast = std::pair(val, UniqueCast<TypeNode>(value.second->subtype[0]->Clone()));
        auto target_subtype = type->subtype[0].get();
        auto subtype_cast_res = Cast(std::move(cast), target_subtype, llvm_type);
        
        // lie about the actual type
        auto res = std::pair(subtype_cast_res.first, UniqueCast<TypeNode>(type->Clone()));
        
        return res;
    }
    
    if (value.second->type == TypeNodeType::SIMD && type->type == TypeNodeType::TENSOR) {
        // check if value's size is equal to the tensors
        auto vector_size = EvaluateInt(value.second->capacity.get());
        
        auto dimensions_tuple = dynamic_cast<TupleNode*>(type->capacity.get());
        
        auto tensor_size = 1;
        for (const auto& dimension : dimensions_tuple->elements) {
            tensor_size *= EvaluateInt(dimension.get()); 
        }
        
        if (vector_size != tensor_size) {
            throw std::runtime_error("tensor and vector must be of equal sizes");
        }
        
        // cast subtypes
        auto cast = std::pair(value.first, UniqueCast<TypeNode>(value.second->subtype[0]->Clone()));
        auto target_subtype = type->subtype[0].get();
        auto subtype_cast_res = Cast(std::move(cast), target_subtype, llvm_type);
        
        // plain reinterpret
        auto res = std::pair(subtype_cast_res.first, UniqueCast<TypeNode>(type->Clone()));
        
        return res;
    }
    
    if (value.second->type == TypeNodeType::TENSOR && type->type == TypeNodeType::SIMD) {
        // check if value's size is equal to the tensors
        auto vector_size = EvaluateInt(type->capacity.get());
        
        auto dimensions_tuple = dynamic_cast<TupleNode*>(value.second->capacity.get());
        
        auto tensor_size = 1;
        for (const auto& dimension : dimensions_tuple->elements) {
            tensor_size *= EvaluateInt(dimension.get()); 
        }
        
        if (vector_size > tensor_size) {
            throw std::runtime_error("tensor and vector must be of equal sizes or lesser");
        }
        
        std::vector<int> shuffle;
        for (auto i = 0; i < vector_size; i++) {
            shuffle.push_back(i);
        }
        
        //shrink
        auto val = builder_->CreateShuffleVector(value.first,
                                                 Constant::getNullValue(GenerateType(value.second.get())),
                                                 shuffle);
        
        // cast subtypes
        auto cast = std::pair(val, UniqueCast<TypeNode>(value.second->subtype[0]->Clone()));
        auto target_subtype = type->subtype[0].get();
        auto subtype_cast_res = Cast(std::move(cast), target_subtype, llvm_type);
        
        // plain reinterpret
        auto res = std::pair(subtype_cast_res.first, UniqueCast<TypeNode>(type->Clone()));
        
        return res;
    }
    
    if (type->type == TypeNodeType::SIMD) {
        // a must be scalar
        
        //alloc a simd vector
        auto elems = EvaluateInt(type->capacity.get());
        auto alloc = builder_->CreateAlloca(GenerateType(type));
        auto casted_val = Cast(std::move(value), type->subtype[0].get());
        for (size_t i = 0; i < elems; i++) {
            auto elem_ptr = builder_->CreateGEP(GenerateType(type->subtype[0].get()), alloc, {builder_->getInt32(i)});
            builder_->CreateStore(casted_val.first, elem_ptr);
        }
        
        auto load = builder_->CreateLoad(GenerateType(type), alloc);
        return {load, std::move(unique_type)};
    }

    if (value.second->Integer() && type->Integer()) {
        bool narrowing = EvaluateSize(value.second.get()) > EvaluateSize(type);
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
        if (value.second->subtype[0]->Equal(type->subtype[0].get(), true)) {
            return {value.first, UniqueCast<TypeNode>(type->Clone())};
        }
    }

    throw std::runtime_error("Invalid cast");
}

std::unique_ptr<TypeNode> LLVMBackend::Promote(TypeNode* a,
                                               TypeNode* b) {
    if (a->Equal(b, true))
        return UniqueCast<TypeNode>(a->Clone());
    if (a->Integer() && b->Integer()) {
        //promote to larger type
        if (EvaluateSize(a) > EvaluateSize(b)) {
            return UniqueCast<TypeNode>(a->Clone());
        }
        return UniqueCast<TypeNode>(b->Clone());
    }
    if (a->Float() && b->Float()) {
        //promote to larger type
        if (EvaluateSize(a) > EvaluateSize(b)) {
            return UniqueCast<TypeNode>(a->Clone());
        }
        return UniqueCast<TypeNode>(b->Clone());
    }
    if (a->Float() && b->Integer()) {
        return UniqueCast<TypeNode>(a->Clone()); // promote to float
    }
    if (b->Float() && a->Integer()) {
        return UniqueCast<TypeNode>(b->Clone()); // promote to float
    }
    
    // vector resizing
    if (a->type == TypeNodeType::SIMD && b->type == TypeNodeType::SIMD) {
        auto a_size = EvaluateInt(a->capacity.get());
        auto b_size = EvaluateInt(b->capacity.get());
        //Grow
        auto final_size = std::max(a_size, b_size);
        return std::make_unique<TypeNode>(TypeNodeType::SIMD,
                                          Promote(a->subtype[0].get(), b->subtype[0].get()),
                                          std::make_unique<IntegerNode>(final_size));
    }
    
    // scalar
    if (a->type == TypeNodeType::SIMD) {
        auto a_size = EvaluateInt(a->capacity.get());
        // b is scalar
        return std::make_unique<TypeNode>(TypeNodeType::SIMD, Promote(a->subtype[0].get(), b), std::make_unique<IntegerNode>(a_size));
    }
    std::cerr << "Unhandled promotion rule, could be possible source of bug" << std::endl;
    return UniqueCast<TypeNode>(a->Clone());
}

std::pair<Value*, std::unique_ptr<TypeNode> > LLVMBackend::GenerateRValue(AstNode* get, const TypeNode* expected) {
    if (auto compound = is<CompoundStatement>(get)) {
        scope_.PushScope();
        std::pair<Value*, std::unique_ptr<TypeNode> > last = {};
        
        bool terminated = false;
        for (const auto& statement: compound->statements) {
            last = GenerateRValue(statement.get(), expected);

            if (builder_->GetInsertBlock()->getTerminator()) {
                terminated = true;
                break;
            }
        }
        if (terminated) {
            auto currentBlock = builder_->GetInsertBlock();
            auto terminator = currentBlock->getTerminator();
            builder_->SetInsertPoint(terminator);
        }
        scope_.PopScope(this, builder_.get(), current_block);
        if (last.first == nullptr) {
            return {};
        }
        if (expected == nullptr) {
            return last;
        }
        if (expected->type == TypeNodeType::VOID) {
            return {};
        }
        
        return Cast(std::move(last), expected);
    }
    if (auto variable = is<VariableNode>(get)) {
        if (variable->type == nullptr) {
            //inferred
            if (variable->value == nullptr)
                throw std::runtime_error("Unable to infer variable type");
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
                                  ? Cast(GenerateRValue(variable->value.get(), variable->type.get()),
                                         variable->type.get()).first
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
            builder_->CreateBr(exit->basic_block);
            return {};
        }
        auto result = GenerateRValue(return_statement->value.get(), expected);
        //result.first
        builder_->CreateStore(result.first, ret, false);
        builder_->CreateBr(exit->basic_block);
        return {};
    }
    if (auto if_statement = is<IfNode>(get)) {
        auto condition = GenerateRValue(if_statement->condition.get(), &BOOLEAN);
        auto then_block =  std::make_shared<Block>(*context_, func, "if.then");
        auto else_block =  std::make_shared<Block>(*context_, func, "if.else");
        auto merge_block = std::make_shared<Block>(*context_, func, "if.merge");

        builder_->CreateCondBr(condition.first, then_block->basic_block, else_block->basic_block);
        current_block->Connect(then_block);
        current_block->Connect(else_block);

        builder_->SetInsertPoint(then_block->basic_block);
        current_block = then_block.get();
        GenerateRValue(if_statement->then.get(), expected);
        if (!then_block->basic_block->getTerminator()) {
            builder_->CreateBr(merge_block->basic_block);
            then_block->Connect(merge_block);
        }

        builder_->SetInsertPoint(else_block->basic_block);
        current_block = else_block.get();
        if (if_statement->otherwise != nullptr) {
            GenerateRValue(if_statement->otherwise.get(), expected);
        }

        if (!else_block->basic_block->getTerminator()) {
            else_block->Connect(merge_block);
            builder_->CreateBr(merge_block->basic_block);
        }

        if (merge_block->basic_block->hasNPredecessors(0)) {
            merge_block->basic_block->eraseFromParent();
        } else {
            builder_->SetInsertPoint(merge_block->basic_block);
            current_block = merge_block.get();
        }
        return {};
    }
    if (auto while_statement = is<WhileNode>(get)) {
        auto cond_block = std::make_shared<Block>(*context_, func, "while.cond");
        auto loop_block = std::make_shared<Block>(*context_, func, "while.loop");
        auto merge_block =std::make_shared<Block>(*context_, func, "while.merg");

        builder_->CreateBr(cond_block->basic_block);
        builder_->SetInsertPoint(cond_block->basic_block);
        current_block = cond_block.get();
        auto condition = GenerateRValue(while_statement->condition.get(), &BOOLEAN);
        builder_->CreateCondBr(condition.first, loop_block->basic_block, merge_block->basic_block);
        cond_block->Connect(loop_block);
        cond_block->Connect(merge_block);

        builder_->SetInsertPoint(loop_block->basic_block);
        current_block = loop_block.get();
        auto result = GenerateRValue(while_statement->body.get(), nullptr);
        builder_->CreateBr(cond_block->basic_block);
        loop_block->Connect(cond_block);

        builder_->SetInsertPoint(merge_block->basic_block);
        current_block = merge_block.get();
        return {};
    }
    if (auto for_statement = is<ForNode>(get)) {
        scope_.PushScope();
        GenerateRValue(for_statement->init.get(), nullptr);
        auto cond_block = std::make_shared<Block>(*context_, func, "for.cond");
        auto loop_block = std::make_shared<Block>(*context_, func, "for.loop");
        auto merge_block = std::make_shared<Block>(*context_, func, "for.merge");
        
        builder_->CreateBr(cond_block->basic_block);
        builder_->SetInsertPoint(cond_block->basic_block);
        current_block = cond_block.get();
        
        auto condition = GenerateRValue(for_statement->condition.get(), &BOOLEAN);
        builder_->CreateCondBr(condition.first, loop_block->basic_block, merge_block->basic_block);
        cond_block->Connect(loop_block);
        cond_block->Connect(merge_block);
        
        builder_->SetInsertPoint(loop_block->basic_block);
        current_block = loop_block.get();
        auto result = GenerateRValue(for_statement->body.get(), expected);
        auto postfix = GenerateRValue(for_statement->postfix.get(), nullptr);
        builder_->CreateBr(cond_block->basic_block);
        loop_block->Connect(cond_block);
        
        builder_->SetInsertPoint(merge_block->basic_block);
        current_block = merge_block.get();
        scope_.PopScope(this, builder_.get(), current_block);
        return {};
        
    }
    if (const auto integer = is<IntegerNode>(get)) {
        Value* val = nullptr;
        if (expected != nullptr && expected->Integer()) {
            switch (expected->type) {
                case TypeNodeType::I8:
                    val = ConstantInt::get(*context_, APInt(8, integer->value, true));
                    break;
                case TypeNodeType::I16:
                    val = ConstantInt::get(*context_, APInt(16, integer->value, true));
                    break;
                case TypeNodeType::I32:
                    val = ConstantInt::get(*context_, APInt(32, integer->value, true));
                    break;
                case TypeNodeType::I64:
                    val = ConstantInt::get(*context_, APInt(64, integer->value, true));
                    break;
                case TypeNodeType::U8:
                    val = ConstantInt::get(*context_, APInt(8, integer->value, false));
                    break;
                case TypeNodeType::U16:
                    val = ConstantInt::get(*context_, APInt(16, integer->value, false));
                    break;
                case TypeNodeType::U32:
                    val = ConstantInt::get(*context_, APInt(32, integer->value, false));
                    break;
                case TypeNodeType::U64:
                    val = ConstantInt::get(*context_, APInt(64, integer->value, false));
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
        if (expected != nullptr && expected->type == TypeNodeType::TENSOR) {
            //assume we're gonna be defining a tensor array
            std::vector<Value*> values;
            size_t size = 0;

            auto tensor_type = expected->subtype[0].get();

            for (const auto& element: tuple->elements) {
                auto value = GenerateRValue(element.get(), tensor_type);
                size += EvaluateSize(value.second.get());
                values.push_back(value.first);
            }

            auto tensor_node = std::make_unique<TypeNode>(TypeNodeType::TENSOR,
                                                        UniqueCast<TypeNode>(tensor_type->Clone()),
                                                        UniqueCast<TupleNode>(expected->capacity->Clone()));
            
            auto type = GenerateType(tensor_node.get());
            auto arr = builder_->CreateAlloca(type);

            for (size_t i = 0; i < values.size(); i++) {
                auto val = values[i];
                auto elem_ptr = builder_->CreateGEP(GenerateType(tensor_type), arr, {builder_->getInt32(i)});
                builder_->CreateStore(val, elem_ptr);
            }

            //load
            auto load = builder_->CreateLoad(type, arr);

            return std::pair{load, std::move(tensor_node)};
        }
        if (expected != nullptr && expected->type == TypeNodeType::SIMD) {
            //assume we're gonna be defining a SIMD array
            std::vector<Value*> values;
            size_t size = 0;

            auto simd_type = expected->subtype[0].get();

            for (const auto& element: tuple->elements) {
                auto value = GenerateRValue(element.get(), simd_type);
                size += EvaluateSize(value.second.get());
                values.push_back(value.first);
            }

            auto simd_node = std::make_unique<TypeNode>(TypeNodeType::SIMD,
                                                        UniqueCast<TypeNode>(simd_type->Clone()),
                                                        std::make_unique<IntegerNode>(values.size()));
            auto type = GenerateType(simd_node.get());
            auto arr = builder_->CreateAlloca(type);

            for (size_t i = 0; i < values.size(); i++) {
                auto val = values[i];
                auto elem_ptr = builder_->CreateGEP(GenerateType(simd_type), arr, {builder_->getInt32(i)});
                builder_->CreateStore(val, elem_ptr);
            }

            //load
            auto load = builder_->CreateLoad(type, arr);

            return std::pair{load, std::move(simd_node)};
        }

        std::vector<Value*> values;
        std::vector<std::unique_ptr<TypeNode> > types;
        bool identical_types = true;
        size_t size = 0;
        for (const auto& element: tuple->elements) {
            auto value = GenerateRValue(element.get(), nullptr);
            if (!types.empty()) {
                if (!types[0]->Equal(value.second.get(), true)) {
                    identical_types = false;
                }
            }
            size += EvaluateSize(value.second.get());
            values.push_back(value.first);
            types.push_back(std::move(value.second));
        }

        if (identical_types && expected == nullptr) {
            //prefer a simd type to a tuple
            auto simd_node = std::make_unique<TypeNode>(TypeNodeType::SIMD,
                                                        UniqueCast<TypeNode>(types[0]->Clone()),
                                                        std::make_unique<IntegerNode>(values.size()));
            auto type = GenerateType(simd_node.get());
            auto arr = builder_->CreateAlloca(type);

            for (size_t i = 0; i < values.size(); i++) {
                auto val = values[i];
                auto elem_ptr = builder_->CreateGEP(GenerateType(types[0].get()), arr, {builder_->getInt32(i)});
                builder_->CreateStore(val, elem_ptr);
            }

            //load
            auto load = builder_->CreateLoad(type, arr);

            return std::pair{load, std::move(simd_node)};
        }
        auto tuple_node = std::make_unique<TypeNode>(TypeNodeType::TUPLE, std::move(types));
        auto type = GenerateType(tuple_node.get());
        auto arr = builder_->CreateAlloca(type);

        for (size_t i = 0; i < values.size(); i++) {
            auto val = values[i];
            auto elem_ptr = builder_->CreateGEP(type, arr, {builder_->getInt32(0), builder_->getInt32(i)});
            builder_->CreateStore(val, elem_ptr);
        }

        //load
        auto load = builder_->CreateLoad(type, arr);

        return std::pair{load, std::move(tuple_node)};
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
        if (!current_block->Valid(var)) {
            throw std::runtime_error("Use of moved value: " + identifier->identifier);
        }
        
        bool reading_owner = type && type->subtype[0] && type->subtype[0]->type == TypeNodeType::OWNER;
        bool consumer_wants_owner = expected && expected->type == TypeNodeType::OWNER;
        
        if (reading_owner && consumer_wants_owner) {
            current_block->Move(var);
        }
        
        auto drill = Drill({var, UniqueCast<TypeNode>(type->Clone())}, expected);
        return drill;
    }
    if (const auto assign = is<AssignNode>(get)) {
        auto target = GenerateLValue(assign->target.get());
        auto type_target = target.second.get();
        while (type_target->subtype[0]->Pointer())
            type_target = type_target->subtype[0].get();
        target = Drill(std::move(target), type_target);
        auto value = GenerateRValue(assign->value.get(), target.second->subtype[0].get());
        builder_->CreateStore(value.first, target.first);
        return value;
    }
    if (const auto binary = is<BinaryNode>(get)) {
        auto left = GenerateRValue(binary->left.get(), nullptr);
        auto right = GenerateRValue(binary->right.get(), nullptr);
        auto left_comp_type = left.second.get();
        auto right_comp_type = left.second.get();

        if (left.second->type == TypeNodeType::SIMD || left.second->type == TypeNodeType::TENSOR) {
            left_comp_type = left.second->subtype[0].get();
        }
        if (right.second->type == TypeNodeType::SIMD || right.second->type == TypeNodeType::TENSOR) {
            right_comp_type = right.second->subtype[0].get();
        }

        if (left.second->type == TypeNodeType::TENSOR) {
            if (binary->type != BinaryNodeType::MULTIPLY)
                throw std::runtime_error("Only multiplication is currently allowed for tensor types");

            if (right.second->type != TypeNodeType::TENSOR) {
                throw std::runtime_error("Tensor's are currently only allowed to perform arithmetic with other tensors");
            }
            //ensure valid sizes
            auto left_capacity_tuple = dynamic_cast<TupleNode*>(left.second->capacity.get());
            auto right_capacity_tuple = dynamic_cast<TupleNode*>(right.second->capacity.get());

            // ensure rank-2 tensor
            if (left_capacity_tuple->elements.size() > 2 || right_capacity_tuple->elements.size() > 2) {
                throw std::runtime_error("Arithmetic is only allowed on rank-2 tensors");
            }

            int64_t left_dimensions[2] = {EvaluateInt(left_capacity_tuple->elements[0].get()), EvaluateInt(left_capacity_tuple->elements[1].get())};
            int64_t right_dimensions[2] = {EvaluateInt(right_capacity_tuple->elements[0].get()), EvaluateInt(right_capacity_tuple->elements[1].get())};


            //ensure valid sizes for tensor multiplication
            // left.w == right.h
            if (left_dimensions[1] != right_dimensions[0]) {
                throw std::runtime_error("Tensor multiplication is only allowed when the width of the left tensor is equal to the height of the right tensor");
            }

            // following wikipedia's definition of a matrix multiplication
            int m = left_dimensions[0];
            int n = left_dimensions[1];
            int p = right_dimensions[1];

            // result is m x p
            int result_dimensions[2] = {m, p};

            std::vector<Value*> result_values;

            auto promotion = Promote(left_comp_type, right_comp_type);

            for (int i = 0; i < m; i++) {
                for (int j = 0; j < p; j++) {
                    Value* sum = nullptr;
                    for (int k = 0; k < n; k++) {
                        int left_index = i * n + k;
                        int right_index = k * p + j;
                        auto left_value = builder_->CreateExtractElement(left.first, builder_->getInt64(left_index));
                        auto right_value = builder_->CreateExtractElement(right.first, builder_->getInt64(right_index));

                        auto left_prom_value = Cast({left_value, UniqueCast<TypeNode>(left_comp_type->Clone())}, promotion.get());
                        auto right_prom_value = Cast({right_value, UniqueCast<TypeNode>(right_comp_type->Clone())}, promotion.get());

                        if (left_prom_value.second->Float()) {
                            auto mul = builder_->CreateFMul(left_prom_value.first, right_prom_value.first);

                            if (sum == nullptr) {
                                sum = mul;
                            } else {
                                sum = builder_->CreateFAdd(sum, mul);
                            }
                        }
                        else {
                            auto mul = builder_->CreateMul(left_prom_value.first, right_prom_value.first);

                            if (sum == nullptr) {
                                sum = mul;
                            } else {
                                sum = builder_->CreateAdd(sum, mul);
                            }
                        }
                    }
                    result_values.push_back(sum);
                }
            }

            // construct the new tensor
            std::vector<std::unique_ptr<ExpressionNode>> tuple_dimensions;
            tuple_dimensions.push_back(std::make_unique<IntegerNode>(m));
            tuple_dimensions.push_back(std::make_unique<IntegerNode>(p));
            auto size_tuple = std::make_unique<TupleNode>(std::move(tuple_dimensions));
            auto type = std::make_unique<TypeNode>(TypeNodeType::TENSOR, UniqueCast<TypeNode>(promotion->Clone()), std::move(size_tuple));

            Value* vec = PoisonValue::get(FixedVectorType::get(GenerateType(promotion.get()), result_values.size()));

            //store all the values
            for (auto i = 0; i < result_values.size(); i++) {
                vec = builder_->CreateInsertElement(vec, result_values[i], i);
            }

            return {vec, std::move(type)};
        }
        if (left_comp_type->Float() || right_comp_type->Float()) {
            auto promotion = Promote(left.second.get(), right.second.get());
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
                default:
                    throw std::runtime_error("Unsupported binary expression type");
            }
        }

        auto promotion = Promote(left.second.get(), right.second.get());
        left = Cast(std::move(left), promotion.get());
        right = Cast(std::move(right), promotion.get());

        bool is_signed = left_comp_type->Signed() || right_comp_type->Signed();
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
        auto x = GenerateRValue(heap->expression.get(), expected->subtype[0].get());
        auto type = GenerateType(x.second.get());
        auto ptr = builder_->CreateMalloc(Type::getInt64Ty(*context_), type,
                                          ConstantInt::get(*context_, APInt(64, EvaluateSize(x.second.get()))),
                                          nullptr);
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
        return {val, UniqueCast<TypeNode>(callee.second->subtype[0]->Clone())};
    }
    if (const auto index = is<IndexNode>(get)) {
        return Drill(GenerateLValue(index), expected);
    }
    if (const auto field = is<FieldNode>(get)) {
        auto location = GenerateLValue(field->object.get());
        while (location.second->subtype[0]->Pointer())
            location = Drill(std::move(location), location.second->subtype[0].get());
        if (location.second->subtype[0]->type == TypeNodeType::SIMD) {
            // Compute swizzle indices
            std::vector<int> swizzle_indices;
            for (const auto& component: field->name) {
                if (component == 'x')
                    swizzle_indices.push_back(0);
                else if (component == 'y')
                    swizzle_indices.push_back(1);
                else if (component == 'z')
                    swizzle_indices.push_back(2);
                else if (component == 'w')
                    swizzle_indices.push_back(3);
                else
                    throw std::runtime_error("Invalid SIMD component: " + std::string(1, component));
            }
            auto result_size = swizzle_indices.size();

            //load vector
            auto load = builder_->CreateLoad(GenerateType(location.second->subtype[0].get()), location.first);
            auto vecType = location.second->subtype[0].get();

            if (result_size == 1) {
                auto result = builder_->CreateExtractElement(load, swizzle_indices[0]);

                auto res = std::pair(result, UniqueCast<TypeNode>(vecType->subtype[0]->Clone()));
                return Drill(std::move(res), expected);
            }

            auto swizzle = builder_->
                    CreateShuffleVector(load, PoisonValue::get(GenerateType(vecType)), swizzle_indices);
            //Generate the new T<N> type
            auto newType = UniqueCast<TypeNode>(vecType->Clone());
            newType->capacity = std::make_unique<IntegerNode>(result_size);

            return {swizzle, std::move(newType)};
        }
        return Drill(GenerateLValue(field), expected);
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
    if (const auto index = is<IndexNode>(get)) {
        auto location = GenerateLValue(index->data.get());
        while (location.second->subtype[0]->Pointer())
            location = Drill(std::move(location), location.second->subtype[0].get());

        if (!location.second->subtype[0]->Indexable())
            throw std::runtime_error("Index operation not supported on non-indexable type");

        auto dereference_type = location.second->subtype[0]->subtype[0].get();
        
        if (location.second->subtype[0]->type == TypeNodeType::TENSOR) {
            std::vector<std::pair<Value*, std::unique_ptr<TypeNode> > > values;
            Value* ind = nullptr;
            
            auto expected = std::make_unique<TypeNode>(TypeNodeType::U64);
            
            auto tuple = dynamic_cast<TupleNode*>(index->index.get());
            if (tuple) {
                //matrix sizes
                std::vector<int64_t> dimensions_offsets;
                int64_t offset = 1;
                auto capacity_tuple = dynamic_cast<TupleNode*>(location.second->subtype[0]->capacity.get());
                if (!capacity_tuple) {
                    throw std::runtime_error("Invalid capacity type");
                }
            
                if (capacity_tuple->elements.size() != tuple->elements.size())
                    throw std::runtime_error("Invalid capacity size");
            
                for (const auto& dimension : capacity_tuple->elements) {
                    dimensions_offsets.push_back(offset);
                    offset *= EvaluateInt(dimension.get());
                }
            
                //calculate offset
                Value* sum = nullptr;
                for (auto i = 0; i < dimensions_offsets.size(); i++) {
                    auto dimension = GenerateRValue(tuple->elements[i].get(), expected.get());
                    auto mul = builder_->CreateMul(dimension.first, builder_->getInt64(dimensions_offsets[i]));
                    if (sum == nullptr) {
                        sum = mul;
                        continue;
                    }
                    sum = builder_->CreateAdd(sum, mul);
                }
                ind = sum;
            }
            else {
                ind = GenerateRValue(index->index.get(), expected.get()).first;
            }
            
            auto result_ptr_type = std::make_unique<TypeNode>(TypeNodeType::BORROW,
                                                              UniqueCast<TypeNode>(dereference_type->Clone()));
            auto element_ptr = builder_->CreateGEP(GenerateType(dereference_type), location.first, {ind});
            return {element_ptr, std::move(result_ptr_type)};
        }

        // 1 dimension
        auto index_value = GenerateRValue(index->index.get(), nullptr);
        
        //TODO: note for later, GEP may confuse the optimizer.
        if (location.second->subtype[0]->type == TypeNodeType::SIMD) {
            //simd's get loaded differently
            auto result_ptr_type = std::make_unique<TypeNode>(TypeNodeType::BORROW,
                                                              UniqueCast<TypeNode>(dereference_type->Clone()));

            auto element_ptr = builder_->CreateGEP(GenerateType(dereference_type), location.first, {index_value.first});
            return {element_ptr, std::move(result_ptr_type)};
        }
        
        
        //tuples get loaded differently
        auto result_ptr_type = std::make_unique<TypeNode>(TypeNodeType::BORROW,
                                                          UniqueCast<TypeNode>(dereference_type->Clone()));

        auto element_ptr = builder_->CreateGEP(GenerateType(location.second->subtype[0].get()), location.first,
                                               {builder_->getInt32(0), index_value.first});
        return {element_ptr, std::move(result_ptr_type)};
    }
    if (const auto field = is<FieldNode>(get)) {
        auto location = GenerateLValue(field->object.get());

        while (location.second->subtype[0]->Pointer())
            location = Drill(std::move(location), location.second->subtype[0].get());

        if (location.second->subtype[0]->type == TypeNodeType::SIMD) {
            // Compute swizzle indices
            std::vector<int> swizzle_indices;
            for (const auto& component: field->name) {
                if (component == 'x')
                    swizzle_indices.push_back(0);
                else if (component == 'y')
                    swizzle_indices.push_back(1);
                else if (component == 'z')
                    swizzle_indices.push_back(2);
                else if (component == 'w')
                    swizzle_indices.push_back(3);
                else
                    throw std::runtime_error("Invalid SIMD component: " + std::string(1, component));
            }
            auto result_size = swizzle_indices.size();

            //load vector
            auto vec_type = location.second->subtype[0].get();
            auto elementType = vec_type->subtype[0].get();

            if (result_size == 1) {
                //getelementptr

                auto element_ptr = builder_->CreateGEP(GenerateType(elementType), location.first,
                                                       {builder_->getInt32(swizzle_indices[0])});
                auto ptr = std::make_unique<TypeNode>(TypeNodeType::BORROW, UniqueCast<TypeNode>(elementType->Clone()));
                auto res = std::pair(element_ptr, std::move(ptr));
                return res;
            }

            auto alloc_type = std::make_unique<TypeNode>(TypeNodeType::SIMD,
                                                         std::make_unique<TypeNode>(
                                                             TypeNodeType::BORROW,
                                                             UniqueCast<TypeNode>(vec_type->subtype[0]->Clone())),
                                                         std::make_unique<IntegerNode>(result_size));
            auto alloc = builder_->CreateAlloca(GenerateType(alloc_type.get()), nullptr);

            // get individual elements
            for (auto i = 0; i < result_size; i++) {
                auto element_ptr = builder_->CreateGEP(GenerateType(elementType), location.first,
                                                       {builder_->getInt32(swizzle_indices[i])});
                auto dest_ptr = builder_->CreateGEP(GenerateType(alloc_type.get()), alloc, {builder_->getInt32(i)});
                builder_->CreateStore(element_ptr, dest_ptr);
            }

            //load it
            auto load = builder_->CreateLoad(GenerateType(alloc_type.get()), alloc);

            return {load, std::move(alloc_type)};
        }
        
        // field access
        
        //load
        auto load_type = location.second->subtype[0].get();
        int index = load_type->GetFieldIndex(field->name);
        if (index != -1) {
            auto gep = builder_->CreateGEP(GenerateType(load_type), location.first, {builder_->getInt32(0), builder_->getInt32(index)});
            auto elem_type = load_type->subtype[index].get();
            return {gep, std::make_unique<TypeNode>(TypeNodeType::BORROW, UniqueCast<TypeNode>(elem_type->Clone()))};
        }
        
        throw std::runtime_error("Field " + field->name + " does not exist");
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

    auto type = std::make_unique<TypeNode>(TypeNodeType::FUNCTION, UniqueCast<TypeNode>(function->type->Clone()));

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

    auto block = std::make_shared<Block>(*context_, func, "entry");
    builder_->SetInsertPoint(block->basic_block);
    current_block = block.get();

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

    exit = std::make_shared<Block>(*context_, nullptr, "exit");

    GenerateRValue(function->body.get(), function->type.get());

    if (!builder_->GetInsertBlock()->getTerminator()) {
        current_block->Connect(exit);
        builder_->CreateBr(exit->basic_block);
    }

    exit->basic_block->insertInto(func);

    builder_->SetInsertPoint(exit->basic_block);
    current_block = exit.get();
    
    scope_.PopScope(this, builder_.get(), current_block);
    
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


    exit = nullptr;
}

void LLVMBackend::GenerateVariable(VariableNode* variable) {
    auto var = module_->getOrInsertGlobal(variable->name, GenerateType(variable->type.get()));
    auto gvar = llvm::cast<GlobalVariable>(var);
    gvar->setInitializer(Constant::getNullValue(GenerateType(variable->type.get())));
    auto ptr = std::make_unique<TypeNode>(TypeNodeType::OWNER, UniqueCast<TypeNode>(variable->type->Clone()));

    scope_.Declare(variable->name, gvar, std::move(ptr));
}

void LLVMBackend::GenerateStructure(StructNode* structure) {
    auto type = StructType::get(*context_, false);
    type->setName(structure->name);
}
