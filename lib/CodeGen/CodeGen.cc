#include "cps/CodeGen.h"
#include "llvm/IR/Verifier.h"

using namespace llvm;
using namespace cps;

CodeGen::CodeGen() {
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("cps_module", *TheContext);
    Builder = std::make_unique<IRBuilder<>>(*TheContext);
    
    SetupExternalFunctions();
}

void CodeGen::SetupExternalFunctions() {
    // printf(i8*, ...)
    std::vector<Type*> PrintfArgs;
    PrintfArgs.push_back(PointerType::getUnqual(*TheContext));
    FunctionType *PrintfType = FunctionType::get(Type::getInt32Ty(*TheContext), PrintfArgs, true);
    PrintfFunc = TheModule->getOrInsertFunction("printf", PrintfType);

    // scanf(i8*, ...)
    FunctionType *ScanfType = FunctionType::get(Type::getInt32Ty(*TheContext), PrintfArgs, true);
    ScanfFunc = TheModule->getOrInsertFunction("scanf", ScanfType);

    // format %d\n and %d
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
        // DECLARE 
        if (auto *Decl = dynamic_cast<DeclareStmtAST*>(Stmt.get())) {
            AllocaInst *Alloca = CreateEntryBlockAlloca(F, Decl->getName());
            NamedValues[Decl->getName()] = Alloca;
            // 0
            Builder->CreateStore(ConstantInt::get(*TheContext, APInt(32, 0)), Alloca);
        }
        // ASSIGNMENT (x <- expr) 
        else if (auto *Assign = dynamic_cast<AssignStmtAST*>(Stmt.get())) {
            Value *Val = emitExpr(Assign->getExpr());
            if (Val) {
                AllocaInst *Alloca = NamedValues[Assign->getName()];
                if (Alloca) Builder->CreateStore(Val, Alloca);
            }
        }
        // INPUT 
        else if (auto *In = dynamic_cast<InputStmtAST*>(Stmt.get())) {
            AllocaInst *Alloca = NamedValues[In->getName()];
            if (Alloca) {
                std::vector<Value*> Args;
                Args.push_back(ScanfFormatStr);
                Args.push_back(Alloca);
                Builder->CreateCall(ScanfFunc, Args);
            }
        }
        // OUTPUT
        else if (auto *Out = dynamic_cast<OutputStmtAST*>(Stmt.get())) {
            Value *Val = emitExpr(Out->getExpr());

            if (Val) {
                // printf("%d\n", val)
                std::vector<Value*> Args;
                Args.push_back(PrintfFormatStr);
                Args.push_back(Val);
                Builder->CreateCall(PrintfFunc, Args);
            }
        }
    }

    // main return 0
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
        default:  
            fprintf(stderr, "Error: Invalid binary operator\n");
            return nullptr;
        }
    }

    return nullptr;
}
