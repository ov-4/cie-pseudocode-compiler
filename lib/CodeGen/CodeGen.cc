#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Instructions.h" 
#include "cps/Lexer.h"

using namespace llvm;
using namespace cps;

CodeGen::~CodeGen() = default;

CodeGen::CodeGen() {
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("cps_module", *TheContext);
    Builder = std::make_unique<IRBuilder<>>(*TheContext);
    
    RuntimeChecker = std::make_unique<RuntimeCheck>(*TheModule, *TheContext, *Builder);
    
    Arrays = std::make_unique<ArrayHandler>(*TheContext, *Builder, *TheModule, NamedValues, *RuntimeChecker);
    
    FuncGen = std::make_unique<FunctionGen>(*TheContext, *TheModule, *Builder, NamedValues);

    IntHandler = std::make_unique<IntegerHandler>(*TheContext, *Builder, *TheModule, NamedValues);
    RealHelper = std::make_unique<RealHandler>(*TheContext, *Builder, *TheModule, NamedValues);
    BoolHandler = std::make_unique<BooleanHandler>(*TheContext, *Builder, *TheModule, NamedValues);
    ArithHandler = std::make_unique<ArithmeticHandler>(*TheContext, *Builder);

    SetupExternalFunctions();
}

void CodeGen::SetupExternalFunctions() {
    std::vector<Type*> PrintfArgs;
    PrintfArgs.push_back(PointerType::getUnqual(*TheContext));
    FunctionType *PrintfType = FunctionType::get(Type::getInt32Ty(*TheContext), PrintfArgs, true);
    PrintfFunc = TheModule->getOrInsertFunction("printf", PrintfType);

    FunctionType *ScanfType = FunctionType::get(Type::getInt32Ty(*TheContext), PrintfArgs, true);
    ScanfFunc = TheModule->getOrInsertFunction("scanf", ScanfType);

    PrintfFormatStr = Builder->CreateGlobalStringPtr("%lld\n", "fmt_nl", 0, TheModule.get());
    PrintfFloatFormatStr = Builder->CreateGlobalStringPtr("%f\n", "fmt_flt", 0, TheModule.get());
    PrintfStringFormatStr = Builder->CreateGlobalStringPtr("%s\n", "fmt_str", 0, TheModule.get());

    ScanfFormatStr = Builder->CreateGlobalStringPtr("%lld", "fmt_in", 0, TheModule.get());
    ScanfFloatFormatStr = Builder->CreateGlobalStringPtr("%lf", "fmt_in_flt", 0, TheModule.get());

    TrueStr = Builder->CreateGlobalStringPtr("TRUE", "str_true", 0, TheModule.get());
    FalseStr = Builder->CreateGlobalStringPtr("FALSE", "str_false", 0, TheModule.get());
    
    Arrays->setupExternalFunctions();
}

AllocaInst *CodeGen::CreateEntryBlockAlloca(Function *TheFunction, const std::string &VarName) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(Type::getInt64Ty(*TheContext), nullptr, VarName);
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

    if (auto *Num = dynamic_cast<IntegerExprAST*>(Expr)) {
        return IntHandler->createLiteral(Num->getVal());
    }

    if (auto *Real = dynamic_cast<RealExprAST*>(Expr)) {
        return RealHelper->createLiteral(Real->getVal());
    }

    if (auto *Bool = dynamic_cast<BooleanExprAST*>(Expr)) {
        return BoolHandler->createLiteral(Bool->getVal());
    }
    
    if (auto *Var = dynamic_cast<VariableExprAST*>(Expr)) {
        Value *A = NamedValues[Var->getName()];
        if (!A) {
            fprintf(stderr, "Error: Unknown variable name %s\n", Var->getName().c_str());
            return nullptr;
        }
        
        Type *AllocType = cast<AllocaInst>(A)->getAllocatedType();
        return Builder->CreateLoad(AllocType, A, Var->getName().c_str());
    }

    if (auto *ArrAcc = dynamic_cast<ArrayAccessExprAST*>(Expr)) {
        return Arrays->emitArrayAccess(ArrAcc, *this);
    }

    if (auto *Unary = dynamic_cast<UnaryExprAST*>(Expr)) {
        Value *Operand = emitExpr(Unary->getOperand());
        if (!Operand) return nullptr;
        
        if (Unary->getOp() == tok_not) {
            if (Operand->getType()->isIntegerTy(64)) {
                Operand = Builder->CreateICmpNE(Operand, ConstantInt::get(*TheContext, APInt(64, 0)), "tobool");
            } else if (Operand->getType()->isDoubleTy()) {
                Operand = Builder->CreateFCmpONE(Operand, ConstantFP::get(*TheContext, APFloat(0.0)), "tobool");
            }
            
            return Builder->CreateNot(Operand, "nottmp");
        }
    }

    if (auto *Bin = dynamic_cast<BinaryExprAST*>(Expr)) {
        Value *L = emitExpr(Bin->getLHS());
        Value *R = emitExpr(Bin->getRHS());
        if (!L || !R) return nullptr;

        return ArithHandler->emitBinaryOp(Bin->getOp(), L, R, Bin->getLine());
    }

    if (auto *Call = dynamic_cast<CallExprAST*>(Expr)) {
        Function *CalleeF = TheModule->getFunction(Call->getCallee());
        std::vector<Value*> Args;
        
        for (unsigned i = 0; i < Call->getArgs().size(); ++i) {
            ExprAST *ArgExpr = Call->getArgs()[i].get();
            bool isByRef = false;

            if (CalleeF && i < CalleeF->arg_size()) {
                if (CalleeF->getArg(i)->getType()->isPointerTy()) {
                    isByRef = true;
                }
            }

            if (isByRef) {
                if (auto *Var = dynamic_cast<VariableExprAST*>(ArgExpr)) {
                    Value *Ptr = NamedValues[Var->getName()];
                    if (!Ptr) {
                        fprintf(stderr, "Error: Unknown variable %s in BYREF call\n", Var->getName().c_str());
                        return nullptr;
                    }
                    Args.push_back(Ptr);
                } else {
                     fprintf(stderr, "Error: BYREF argument must be a variable.\n");
                     return nullptr;
                }
            } else {
                Args.push_back(emitExpr(ArgExpr));
            }
        }
        return FuncGen->emitCallExpr(Call, Args);
    }

    return nullptr;
}

void CodeGen::emitIfStmt(IfStmtAST *Stmt) {
    Value *CondV = emitExpr(Stmt->getCond());
    if (!CondV) return;

    if (CondV->getType()->isIntegerTy(64))
        CondV = Builder->CreateICmpNE(CondV, ConstantInt::get(*TheContext, APInt(64, 0)), "ifcond");
    if (CondV->getType()->isIntegerTy(1))
         CondV = Builder->CreateICmpNE(CondV, ConstantInt::get(*TheContext, APInt(1, 0)), "ifcond");

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
    if (CondV->getType()->isIntegerTy(64))
        CondV = Builder->CreateICmpNE(CondV, ConstantInt::get(*TheContext, APInt(64, 0)), "loopcond");
    if (CondV->getType()->isIntegerTy(1))
        CondV = Builder->CreateICmpNE(CondV, ConstantInt::get(*TheContext, APInt(1, 0)), "loopcond");
    
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
    
    if (CondV->getType()->isIntegerTy(64))
        CondV = Builder->CreateICmpNE(CondV, ConstantInt::get(*TheContext, APInt(64, 0)), "untilcond");
    if (CondV->getType()->isIntegerTy(1))
        CondV = Builder->CreateICmpNE(CondV, ConstantInt::get(*TheContext, APInt(1, 0)), "untilcond");

    Builder->CreateCondBr(CondV, AfterBB, LoopBB);

    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitForStmt(ForStmtAST *Stmt) {
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    std::string VarName = Stmt->getVarName();
    
    Value *StartVal = emitExpr(Stmt->getStart());
    if (!StartVal) return;

    Value *Alloca = NamedValues[VarName];
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

    Value *CurVar = Builder->CreateLoad(Type::getInt64Ty(*TheContext), Alloca, VarName.c_str());
    Value *EndVal = emitExpr(Stmt->getEnd());
    if (!EndVal) return;

    bool isNegativeStep = false;
    if (Stmt->getStep()) {
        if (auto *Num = dynamic_cast<IntegerExprAST*>(Stmt->getStep())) {
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
        StepVal = ConstantInt::get(*TheContext, APInt(64, 1));
    }
    
    Value *CurValForInc = Builder->CreateLoad(Type::getInt64Ty(*TheContext), Alloca, VarName.c_str());
    Value *NextVal = Builder->CreateAdd(CurValForInc, StepVal, "nextval");
    Builder->CreateStore(NextVal, Alloca);
    Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitStmt(StmtAST *Stmt) {
    if (!Stmt) return;

    if (auto *ArrDecl = dynamic_cast<ArrayDeclareStmtAST*>(Stmt)) {
        Arrays->emitArrayDeclare(ArrDecl, *this);
        return;
    }
    
    if (auto *ArrAssign = dynamic_cast<ArrayAssignStmtAST*>(Stmt)) {
        Arrays->emitArrayAssign(ArrAssign, *this);
        return;
    }

    if (auto *FuncDef = dynamic_cast<FunctionDefAST*>(Stmt)) {
        BasicBlock *SavedBlock = Builder->GetInsertBlock();

        FuncGen->emitFunctionDef(FuncDef, [this](StmtAST *S) { 
            this->emitStmt(S); 
        });

        if (SavedBlock) Builder->SetInsertPoint(SavedBlock);
        
        return;
    }

    else if (auto *Call = dynamic_cast<CallStmtAST*>(Stmt)) {
        Function *CalleeF = TheModule->getFunction(Call->getCallee());
        std::vector<Value*> Args;
        
        for (unsigned i = 0; i < Call->getArgs().size(); ++i) {
            ExprAST *ArgExpr = Call->getArgs()[i].get();
            bool isByRef = false;
            
            if (CalleeF && i < CalleeF->arg_size()) {
                if (CalleeF->getArg(i)->getType()->isPointerTy()) {
                    isByRef = true;
                }
            }

            if (isByRef) {
                if (auto *Var = dynamic_cast<VariableExprAST*>(ArgExpr)) {
                    Value *Ptr = NamedValues[Var->getName()];
                    if (!Ptr) {
                        fprintf(stderr, "Error: Unknown variable %s in BYREF call\n", Var->getName().c_str());
                        return;
                    }
                    Args.push_back(Ptr);
                } else {
                     fprintf(stderr, "Error: BYREF argument must be a variable.\n");
                     return;
                }
            } else {
                Args.push_back(emitExpr(ArgExpr));
            }
        }
        FuncGen->emitCallStmt(Call, Args);
        return;
    }

    else if (auto *Ret = dynamic_cast<ReturnStmtAST*>(Stmt)) {
        Value *RetVal = nullptr;
        if (Ret->getRetVal()) {
            RetVal = emitExpr(Ret->getRetVal());
        }
        FuncGen->emitReturn(Ret, RetVal);
        return;
    }

    if (auto *Decl = dynamic_cast<DeclareStmtAST*>(Stmt)) {
        if (Decl->getType() == "INTEGER") IntHandler->emitDeclare(Decl->getName());
        else if (Decl->getType() == "REAL") RealHelper->emitDeclare(Decl->getName());
        else if (Decl->getType() == "BOOLEAN") BoolHandler->emitDeclare(Decl->getName());
        else {
            fprintf(stderr, "Unknown type %s\n", Decl->getType().c_str());
        }
    }
    else if (auto *Assign = dynamic_cast<AssignStmtAST*>(Stmt)) {
        Value *Val = emitExpr(Assign->getExpr());
        if (Val) {
            Value *Alloca = NamedValues[Assign->getName()];
            if (!Alloca) {
                fprintf(stderr, "Error: Unknown variable name %s\n", Assign->getName().c_str());
                return;
            }
            
            AllocaInst *AI = cast<AllocaInst>(Alloca);
            
            if (AI->getAllocatedType() == Type::getDoubleTy(*TheContext) && Val->getType()->isIntegerTy(64)) {
                Val = Builder->CreateSIToFP(Val, Type::getDoubleTy(*TheContext));
            }
            
            if (AI->getAllocatedType() == Type::getInt64Ty(*TheContext) && Val->getType()->isIntegerTy(1)) {
                 Val = Builder->CreateZExt(Val, Type::getInt64Ty(*TheContext));
            }

            Builder->CreateStore(Val, Alloca);
        }
    }
    else if (auto *In = dynamic_cast<InputStmtAST*>(Stmt)) {
        Value *Alloca = NamedValues[In->getName()];
        if (Alloca) {
            AllocaInst *AI = cast<AllocaInst>(Alloca);
            std::vector<Value*> Args;
            
            if (AI->getAllocatedType()->isDoubleTy()) {
                 Args.push_back(ScanfFloatFormatStr); 
                 Args.push_back(Alloca); 
                 Builder->CreateCall(ScanfFunc, Args);
            } else if (AI->getAllocatedType()->isIntegerTy(1)) {
                 AllocaInst *TempInt = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(), "tmp_bool_input");
                 
                 Args.push_back(ScanfFormatStr); 
                 Args.push_back(TempInt);
                 Builder->CreateCall(ScanfFunc, Args);

                 Value *Val = Builder->CreateLoad(Type::getInt64Ty(*TheContext), TempInt);
                 Value *BoolVal = Builder->CreateICmpNE(Val, ConstantInt::get(*TheContext, APInt(64, 0)), "bool_cast");
                 
                 Builder->CreateStore(BoolVal, Alloca);
            } else {
                 Args.push_back(ScanfFormatStr);
                 Args.push_back(Alloca); 
                 Builder->CreateCall(ScanfFunc, Args);
            }
        }
    }
    else if (auto *Out = dynamic_cast<OutputStmtAST*>(Stmt)) {
        bool handled = Arrays->tryEmitArrayOutput(Out->getExpr(), *this, PrintfFunc, PrintfFormatStr);
        
        if (!handled) {
            Value *Val = emitExpr(Out->getExpr());
            if (Val) {
                std::vector<Value*> Args;
                if (Val->getType()->isDoubleTy()) {
                     Args.push_back(PrintfFloatFormatStr);
                     Args.push_back(Val);
                } else if (Val->getType()->isIntegerTy(1)) {
                     Args.push_back(PrintfStringFormatStr);
                     Value *StrPtr = Builder->CreateSelect(Val, TrueStr, FalseStr);
                     Args.push_back(StrPtr);
                } else {
                     Args.push_back(PrintfFormatStr);
                     Args.push_back(Val);
                }
                
                Builder->CreateCall(PrintfFunc, Args);
            }
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
