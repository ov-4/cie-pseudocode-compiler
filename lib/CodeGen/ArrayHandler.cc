#include "cps/ArrayHandler.h"
#include "cps/CodeGen.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"

using namespace llvm;
using namespace cps;

ArrayHandler::ArrayHandler(LLVMContext &C, IRBuilder<> &B, Module &M, std::map<std::string, AllocaInst*> &NV, RuntimeCheck &RC)
    : TheContext(&C), Builder(&B), TheModule(&M), NamedValues(&NV), RuntimeChecker(RC) {
    setupExternalFunctions();
}

void ArrayHandler::setupExternalFunctions() {
    std::vector<Type*> MallocArgs;
    MallocArgs.push_back(Type::getInt64Ty(*TheContext));
    
    FunctionType *MallocType = FunctionType::get(PointerType::getUnqual(*TheContext), MallocArgs, false);
    MallocFunc = TheModule->getOrInsertFunction("malloc", MallocType);

    std::vector<Type*> FreeArgs;
    FreeArgs.push_back(PointerType::getUnqual(*TheContext));
    FunctionType *FreeType = FunctionType::get(Type::getVoidTy(*TheContext), FreeArgs, false);
    FreeFunc = TheModule->getOrInsertFunction("free", FreeType);
}

void ArrayHandler::emitArrayDeclare(ArrayDeclareStmtAST *Stmt, CodeGen &CG) {
    std::string Name = Stmt->getName();
    int Rank = Stmt->getBounds().size();
    
    std::vector<Value*> Lows, Highs, Dims;
    Value *TotalElements = ConstantInt::get(*TheContext, APInt(64, 1));
    
    for (const auto &Pair : Stmt->getBounds()) {
        Value *L = CG.emitExpr(Pair.first.get());
        Value *R = CG.emitExpr(Pair.second.get());
        
        if (!L || !R) return;
        
        Value *L64 = Builder->CreateSExt(L, Type::getInt64Ty(*TheContext));
        Value *R64 = Builder->CreateSExt(R, Type::getInt64Ty(*TheContext));
        
        Lows.push_back(L);
        Highs.push_back(R);
        
        Value *Diff = Builder->CreateSub(R64, L64);
        Value *DimSize = Builder->CreateAdd(Diff, ConstantInt::get(*TheContext, APInt(64, 1)));
        Dims.push_back(DimSize);
        
        TotalElements = Builder->CreateMul(TotalElements, DimSize);
    }
    
    std::vector<Value*> Multipliers(Rank);
    Multipliers[Rank - 1] = ConstantInt::get(*TheContext, APInt(32, 1));
    
    for (int i = Rank - 2; i >= 0; --i) {
        Value *NextMult = Multipliers[i+1];
        Value *NextDimSize = Dims[i+1];
        Value *NextDimSize32 = Builder->CreateTrunc(NextDimSize, Type::getInt32Ty(*TheContext));
        Multipliers[i] = Builder->CreateMul(NextMult, NextDimSize32);
    }

    ArrayMetadata Meta;
    Meta.Rank = Rank;
    Meta.ElementType = Type::getInt32Ty(*TheContext);
    Meta.LowerBounds = Lows;
    Meta.UpperBounds = Highs;
    Meta.Multipliers = Multipliers;
    ArrayTable[Name] = Meta;
    

    Value *ElemSize = ConstantInt::get(*TheContext, APInt(64, 4)); 
    Value *TotalBytes = Builder->CreateMul(TotalElements, ElemSize);
    
    CallInst *Ptr = Builder->CreateCall(MallocFunc, TotalBytes);
    
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    
    AllocaInst *Alloca = TmpB.CreateAlloca(PointerType::getUnqual(*TheContext), nullptr, Name);
    
    Builder->CreateStore(Ptr, Alloca);
    (*NamedValues)[Name] = Alloca;
}

Value* ArrayHandler::computeFlatIndex(const std::string &Name, const std::vector<Value*> &Indices) {
    if (ArrayTable.find(Name) == ArrayTable.end()) return nullptr;
    const auto &Meta = ArrayTable[Name];
    
    Value *Offset = ConstantInt::get(*TheContext, APInt(32, 0));
    
    for (size_t i = 0; i < Indices.size(); ++i) {
        Value *Idx = Indices[i];
        Value *Lower = Meta.LowerBounds[i];
        Value *Diff = Builder->CreateSub(Idx, Lower);
        Value *Term = Builder->CreateMul(Diff, Meta.Multipliers[i]);
        Offset = Builder->CreateAdd(Offset, Term);
    }
    return Offset;
}

Value* ArrayHandler::emitArrayAccess(ArrayAccessExprAST *Expr, CodeGen &CG) {
    std::string Name = Expr->getName();
    if (ArrayTable.find(Name) == ArrayTable.end()) {
        fprintf(stderr, "Error: Undeclared array %s\n", Name.c_str());
        return nullptr;
    }
    
    const auto &Meta = ArrayTable[Name];
    if (Expr->getIndices().size() != Meta.Rank) {
        fprintf(stderr, "Error: Incorrect number of indices for %s\n", Name.c_str());
        return nullptr;
    }

    std::vector<Value*> Indices;
    for (size_t i = 0; i < Expr->getIndices().size(); ++i) {
        Value *Idx = CG.emitExpr(Expr->getIndices()[i].get());
        Indices.push_back(Idx);

        Value *Lower = Meta.LowerBounds[i];
        Value *Upper = Meta.UpperBounds[i];
        RuntimeChecker.emitIndexCheck(Idx, Lower, Upper, Expr->getLine());
    }
    
    Value *Offset = computeFlatIndex(Name, Indices);
    
    AllocaInst *Alloca = (*NamedValues)[Name];
    Value *RawPtr = Builder->CreateLoad(PointerType::getUnqual(*TheContext), Alloca);
    Value *IntPtr = Builder->CreateBitCast(RawPtr, PointerType::getUnqual(*TheContext));
    Value *ElemPtr = Builder->CreateGEP(Type::getInt32Ty(*TheContext), IntPtr, Offset);
    
    return Builder->CreateLoad(Type::getInt32Ty(*TheContext), ElemPtr);
}

void ArrayHandler::emitArrayAssign(ArrayAssignStmtAST *Stmt, CodeGen &CG) {
    std::string Name = Stmt->getName();
    if (ArrayTable.find(Name) == ArrayTable.end()) {
        fprintf(stderr, "Error: Undeclared array %s\n", Name.c_str());
        return;
    }
    
    std::vector<Value*> Indices;
    for (size_t i = 0; i < Stmt->getIndices().size(); ++i) {
        Value *Idx = CG.emitExpr(Stmt->getIndices()[i].get());
        Indices.push_back(Idx);

        const auto &Meta = ArrayTable[Name];
        Value *Lower = Meta.LowerBounds[i];
        Value *Upper = Meta.UpperBounds[i];
        RuntimeChecker.emitIndexCheck(Idx, Lower, Upper, Stmt->getLine());
    }
    
    Value *Val = CG.emitExpr(Stmt->getExpr());
    Value *Offset = computeFlatIndex(Name, Indices);
    
    AllocaInst *Alloca = (*NamedValues)[Name];
    Value *RawPtr = Builder->CreateLoad(PointerType::getUnqual(*TheContext), Alloca);
    Value *IntPtr = Builder->CreateBitCast(RawPtr, PointerType::getUnqual(*TheContext));
    Value *ElemPtr = Builder->CreateGEP(Type::getInt32Ty(*TheContext), IntPtr, Offset);
    
    Builder->CreateStore(Val, ElemPtr);
}

bool ArrayHandler::tryEmitArrayOutput(ExprAST *Expr, CodeGen &CG, llvm::FunctionCallee PrintfFunc, llvm::Value* FmtStr) {
    std::string Name;
    std::vector<Value*> Indices;
    
    if (auto *Var = dynamic_cast<VariableExprAST*>(Expr)) {
        Name = Var->getName();
        if (ArrayTable.find(Name) == ArrayTable.end()) return false; 
    } 
    else if (auto *Acc = dynamic_cast<ArrayAccessExprAST*>(Expr)) {
        Name = Acc->getName();
        if (ArrayTable.find(Name) == ArrayTable.end()) return false;
        
        for (const auto &Idx : Acc->getIndices()) {
            Indices.push_back(CG.emitExpr(Idx.get()));
        }
    } else {
        return false;
    }
    
    const auto &Meta = ArrayTable[Name];
    
    if (Indices.size() == Meta.Rank) {
        return false;
    }
    
    emitPrintLoop(Name, Indices.size(), Indices, PrintfFunc, FmtStr);
    return true;
}

void ArrayHandler::emitPrintLoop(const std::string &Name, int CurrentDim, std::vector<Value*> CurrentIndices, llvm::FunctionCallee PrintfFunc, llvm::Value* FmtStr) {
    const auto &Meta = ArrayTable[Name];
    
    if (CurrentDim == Meta.Rank) {
        Value *Offset = computeFlatIndex(Name, CurrentIndices);
        
        AllocaInst *Alloca = (*NamedValues)[Name];
        Value *RawPtr = Builder->CreateLoad(PointerType::getUnqual(*TheContext), Alloca);
        Value *IntPtr = Builder->CreateBitCast(RawPtr, PointerType::getUnqual(*TheContext));
        Value *ElemPtr = Builder->CreateGEP(Type::getInt32Ty(*TheContext), IntPtr, Offset);
        Value *Val = Builder->CreateLoad(Type::getInt32Ty(*TheContext), ElemPtr);
        
        std::vector<Value*> Args;
        Args.push_back(FmtStr);
        Args.push_back(Val);
        Builder->CreateCall(PrintfFunc, Args);
        return;
    }
    
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "arr_loop", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "arr_after", TheFunction);
    
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    AllocaInst *LoopVar = TmpB.CreateAlloca(Type::getInt32Ty(*TheContext), nullptr, "idx");
    
    Value *Start = Meta.LowerBounds[CurrentDim];
    Value *End = Meta.UpperBounds[CurrentDim];
    
    Builder->CreateStore(Start, LoopVar);
    Builder->CreateBr(LoopBB);
    
    Builder->SetInsertPoint(LoopBB);
    Value *CurVal = Builder->CreateLoad(Type::getInt32Ty(*TheContext), LoopVar);
    
    std::vector<Value*> NextIndices = CurrentIndices;
    NextIndices.push_back(CurVal);
    emitPrintLoop(Name, CurrentDim + 1, NextIndices, PrintfFunc, FmtStr);
    
    Value *NextVal = Builder->CreateAdd(CurVal, ConstantInt::get(*TheContext, APInt(32, 1)));
    Builder->CreateStore(NextVal, LoopVar);
    
    Value *Cond = Builder->CreateICmpSLE(NextVal, End);
    Builder->CreateCondBr(Cond, LoopBB, AfterBB);
    
    Builder->SetInsertPoint(AfterBB);
}
