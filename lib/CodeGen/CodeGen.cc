#include "cps/CodeGen.h"
#include "llvm/IR/Verifier.h"
#include "cps/Lexer.h"

using namespace llvm;
using namespace cps;

CodeGen::CodeGen() {
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("cps_module", *TheContext);
    Builder = std::make_unique<IRBuilder<>>(*TheContext);
    
    SetupExternalFunctions();
}

void CodeGen::SetupExternalFunctions() {
    std::vector<Type*> PrintfArgs;
    PrintfArgs.push_back(PointerType::getUnqual(*TheContext));
    FunctionType *PrintfType = FunctionType::get(Type::getInt32Ty(*TheContext), PrintfArgs, true);
    PrintfFunc = TheModule->getOrInsertFunction("printf", PrintfType);

    FunctionType *ScanfType = FunctionType::get(Type::getInt32Ty(*TheContext), PrintfArgs, true);
    ScanfFunc = TheModule->getOrInsertFunction("scanf", ScanfType);

    PrintfFormatStr = Builder->CreateGlobalStringPtr("%d\n", "fmt_nl", 0, TheModule.get());
    ScanfFormatStr = Builder->CreateGlobalStringPtr("%d", "fmt_in", 0, TheModule.get());
}

AllocaInst *CodeGen::CreateEntryBlockAlloca(Function *TheFunction, const std::string &VarName) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(Type::getInt32Ty(*TheContext), nullptr, VarName);
}

void CodeGen::compile(const std::vector<std::unique_ptr<StmtAST>> &Statements) {
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(*TheContext), false);
    Function *F = Function::Create(FT, Function::ExternalLinkage, "main", TheModule.get());
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", F);
    Builder->SetInsertPoint(BB);

    for (const auto &Stmt : Statements) {
        emitStmt(Stmt.get());
    }

    if (!Builder->GetInsertBlock()->getTerminator())
        Builder->CreateRet(ConstantInt::get(*TheContext, APInt(32, 0)));
        
    verifyFunction(*F);
}

void CodeGen::print() {
    TheModule->print(errs(), nullptr);
}

Value *CodeGen::emitExpr(ExprAST *Expr) {
    if (!Expr) return nullptr;

    if (auto *Num = dynamic_cast<NumberExprAST*>(Expr)) {
        return ConstantInt::get(*TheContext, APInt(32, Num->getVal(), true)); 
    }
    
    if (auto *Var = dynamic_cast<VariableExprAST*>(Expr)) {
        AllocaInst *A = NamedValues[Var->getName()];
        if (!A) {
            fprintf(stderr, "Error: Unknown variable name %s\n", Var->getName().c_str());
            return nullptr;
        }
        return Builder->CreateLoad(Type::getInt32Ty(*TheContext), A, Var->getName().c_str());
    }

    if (auto *Bin = dynamic_cast<BinaryExprAST*>(Expr)) {
        Value *L = emitExpr(Bin->getLHS());
        Value *R = emitExpr(Bin->getRHS());
        if (!L || !R) return nullptr;

        switch (Bin->getOp()) {
        case '+': return Builder->CreateAdd(L, R, "addtmp");
        case '-': return Builder->CreateSub(L, R, "subtmp");
        case '*': return Builder->CreateMul(L, R, "multmp");
        case '/': return Builder->CreateSDiv(L, R, "divtmp");
        case tok_eq: return Builder->CreateICmpEQ(L, R, "eqtmp");
        case tok_ne: return Builder->CreateICmpNE(L, R, "netmp");
        case '<':    return Builder->CreateICmpSLT(L, R, "slttmp");
        case '>':    return Builder->CreateICmpSGT(L, R, "sgttmp");
        case tok_le: return Builder->CreateICmpSLE(L, R, "sletmp");
        case tok_ge: return Builder->CreateICmpSGE(L, R, "sgetmp");
        default:  
            fprintf(stderr, "Error: Invalid binary operator\n");
            return nullptr;
        }
    }
    return nullptr;
}

void CodeGen::emitIfStmt(IfStmtAST *Stmt) {
    Value *CondV = emitExpr(Stmt->getCond());
    if (!CondV) return;

    if (CondV->getType()->isIntegerTy(32))
        CondV = Builder->CreateICmpNE(CondV, ConstantInt::get(*TheContext, APInt(32, 0)), "ifcond");

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else", TheFunction);
    BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont", TheFunction);

    Builder->CreateCondBr(CondV, ThenBB, ElseBB);

    Builder->SetInsertPoint(ThenBB);
    for (const auto &S : Stmt->getThenStmts()) emitStmt(S.get());
    if (!Builder->GetInsertBlock()->getTerminator()) Builder->CreateBr(MergeBB);

    Builder->SetInsertPoint(ElseBB);
    for (const auto &S : Stmt->getElseStmts()) emitStmt(S.get());
    if (!Builder->GetInsertBlock()->getTerminator()) Builder->CreateBr(MergeBB);

    Builder->SetInsertPoint(MergeBB);
}

void CodeGen::emitWhileStmt(WhileStmtAST *Stmt) {
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    
    BasicBlock *CondBB = BasicBlock::Create(*TheContext, "whilecond", TheFunction);
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "whileloop", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "whilecont", TheFunction);

    Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(CondBB);
    Value *CondV = emitExpr(Stmt->getCond());
    if (!CondV) return;
    if (CondV->getType()->isIntegerTy(32))
        CondV = Builder->CreateICmpNE(CondV, ConstantInt::get(*TheContext, APInt(32, 0)), "loopcond");
    
    Builder->CreateCondBr(CondV, LoopBB, AfterBB);

    Builder->SetInsertPoint(LoopBB);
    for (const auto &S : Stmt->getBody()) emitStmt(S.get());
    
    if (!Builder->GetInsertBlock()->getTerminator())
        Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitRepeatStmt(RepeatStmtAST *Stmt) {
    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "repeatloop", TheFunction);
    BasicBlock *CondBB = BasicBlock::Create(*TheContext, "repeatcond", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "repeatcont", TheFunction);

    Builder->CreateBr(LoopBB);

    Builder->SetInsertPoint(LoopBB);
    for (const auto &S : Stmt->getBody()) emitStmt(S.get());
    if (!Builder->GetInsertBlock()->getTerminator())
        Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(CondBB);
    Value *CondV = emitExpr(Stmt->getCond());
    if (!CondV) return;
    
    if (CondV->getType()->isIntegerTy(32))
        CondV = Builder->CreateICmpNE(CondV, ConstantInt::get(*TheContext, APInt(32, 0)), "untilcond");

    Builder->CreateCondBr(CondV, AfterBB, LoopBB);

    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitForStmt(ForStmtAST *Stmt) {
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    std::string VarName = Stmt->getVarName();
    
    Value *StartVal = emitExpr(Stmt->getStart());
    if (!StartVal) return;

    AllocaInst *Alloca = NamedValues[VarName];
    if (!Alloca) {
        fprintf(stderr, "Error: Unknown variable in FOR loop %s\n", VarName.c_str());
        return;
    }
    Builder->CreateStore(StartVal, Alloca);

    BasicBlock *CondBB = BasicBlock::Create(*TheContext, "forcond", TheFunction);
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "forloop", TheFunction);
    BasicBlock *IncBB = BasicBlock::Create(*TheContext, "forinc", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "forcont", TheFunction);

    Builder->CreateBr(CondBB);
    Builder->SetInsertPoint(CondBB);

    Value *CurVar = Builder->CreateLoad(Type::getInt32Ty(*TheContext), Alloca, VarName.c_str());
    Value *EndVal = emitExpr(Stmt->getEnd());
    if (!EndVal) return;

    bool isNegativeStep = false;
    if (Stmt->getStep()) {
        if (auto *Num = dynamic_cast<NumberExprAST*>(Stmt->getStep())) {
            if (Num->getVal() < 0) isNegativeStep = true;
        }
    }

    Value *CondV;
    if (isNegativeStep) {
        CondV = Builder->CreateICmpSGE(CurVar, EndVal, "forcond_ge");
    } else {
        CondV = Builder->CreateICmpSLE(CurVar, EndVal, "forcond_le");
    }

    Builder->CreateCondBr(CondV, LoopBB, AfterBB);

    Builder->SetInsertPoint(LoopBB);
    for (const auto &S : Stmt->getBody()) emitStmt(S.get());
    if (!Builder->GetInsertBlock()->getTerminator())
        Builder->CreateBr(IncBB);

    Builder->SetInsertPoint(IncBB);
    Value *StepVal;
    if (Stmt->getStep()) {
        StepVal = emitExpr(Stmt->getStep());
    } else {
        StepVal = ConstantInt::get(*TheContext, APInt(32, 1));
    }
    
    Value *CurValForInc = Builder->CreateLoad(Type::getInt32Ty(*TheContext), Alloca, VarName.c_str());
    Value *NextVal = Builder->CreateAdd(CurValForInc, StepVal, "nextval");
    Builder->CreateStore(NextVal, Alloca);
    Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitStmt(StmtAST *Stmt) {
    if (!Stmt) return;

    if (auto *Decl = dynamic_cast<DeclareStmtAST*>(Stmt)) {
        Function *TheFunction = Builder->GetInsertBlock()->getParent();
        AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Decl->getName());
        NamedValues[Decl->getName()] = Alloca;
        Builder->CreateStore(ConstantInt::get(*TheContext, APInt(32, 0)), Alloca);
    }
    else if (auto *Assign = dynamic_cast<AssignStmtAST*>(Stmt)) {
        Value *Val = emitExpr(Assign->getExpr());
        if (Val) {
            AllocaInst *Alloca = NamedValues[Assign->getName()];
            if (!Alloca) {
                fprintf(stderr, "Error: Unknown variable name %s\n", Assign->getName().c_str());
                return;
            }
            Builder->CreateStore(Val, Alloca);
        }
    }
    else if (auto *In = dynamic_cast<InputStmtAST*>(Stmt)) {
        AllocaInst *Alloca = NamedValues[In->getName()];
        if (Alloca) {
            std::vector<Value*> Args;
            Args.push_back(ScanfFormatStr);
            Args.push_back(Alloca); 
            Builder->CreateCall(ScanfFunc, Args);
        }
    }
    else if (auto *Out = dynamic_cast<OutputStmtAST*>(Stmt)) {
        Value *Val = emitExpr(Out->getExpr());
        if (Val) {
            std::vector<Value*> Args;
            Args.push_back(PrintfFormatStr);
            Args.push_back(Val);
            Builder->CreateCall(PrintfFunc, Args);
        }
    }
    else if (auto *IfStmt = dynamic_cast<IfStmtAST*>(Stmt)) {
        emitIfStmt(IfStmt);
    }
    else if (auto *WhileStmt = dynamic_cast<WhileStmtAST*>(Stmt)) {
        emitWhileStmt(WhileStmt);
    }
    else if (auto *RepeatStmt = dynamic_cast<RepeatStmtAST*>(Stmt)) {
        emitRepeatStmt(RepeatStmt);
    }
    else if (auto *ForStmt = dynamic_cast<ForStmtAST*>(Stmt)) {
        emitForStmt(ForStmt);
    }
}
