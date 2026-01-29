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
            Value *Val = nullptr;
            if (auto *Num = dynamic_cast<NumberExprAST*>(Assign->getExpr())) {
                Val = ConstantInt::get(*TheContext, APInt(32, Num->getVal()));
            } else if (auto *Var = dynamic_cast<VariableExprAST*>(Assign->getExpr())) {
                 AllocaInst *A = NamedValues[Var->getName()];
                 Val = Builder->CreateLoad(Type::getInt32Ty(*TheContext), A, Var->getName());
            }

            AllocaInst *Alloca = NamedValues[Assign->getName()];
            if (Alloca && Val) {
                Builder->CreateStore(Val, Alloca);
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
            Value *Val = nullptr;
            if (auto *Num = dynamic_cast<NumberExprAST*>(Out->getExpr())) {
                Val = ConstantInt::get(*TheContext, APInt(32, Num->getVal()));
            } else if (auto *Var = dynamic_cast<VariableExprAST*>(Out->getExpr())) {
                 AllocaInst *A = NamedValues[Var->getName()];
                 Val = Builder->CreateLoad(Type::getInt32Ty(*TheContext), A, Var->getName());
            }

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
