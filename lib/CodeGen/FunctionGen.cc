#include "cps/FunctionGen.h"
#include "llvm/IR/Verifier.h"
#include <iostream>

using namespace llvm;
using namespace cps;

Type *FunctionGen::getLLVMType(const std::string &TypeName) {
    if (TypeName == "INTEGER") return Type::getInt64Ty(Context);
    if (TypeName == "BOOLEAN") return Type::getInt1Ty(Context);
    if (TypeName == "REAL")    return Type::getDoubleTy(Context);
    if (TypeName == "VOID")    return Type::getVoidTy(Context);
    return Type::getInt64Ty(Context);
}

void FunctionGen::createArgumentAllocas(Function *F, const std::vector<std::pair<std::string, std::string>> &Args) {
    Function::arg_iterator AI = F->arg_begin();
    for (unsigned Idx = 0, E = Args.size(); Idx != E; ++Idx, ++AI) {
        std::string ArgName = Args[Idx].first;
        std::string ArgTypeStr = Args[Idx].second;
        
        Type *ArgType = getLLVMType(ArgTypeStr);
        IRBuilder<> TmpB(&F->getEntryBlock(), F->getEntryBlock().begin());
        AllocaInst *Alloca = TmpB.CreateAlloca(ArgType, nullptr, ArgName);
        
        Value *ArgVal = &(*AI);
        Builder.CreateStore(ArgVal, Alloca);
        
        NamedValues[ArgName] = Alloca; 
    }
}

Function *FunctionGen::emitPrototype(PrototypeAST *Proto) {
    std::vector<Type*> ArgTypes;
    for (const auto &Arg : Proto->getArgs()) {
        ArgTypes.push_back(getLLVMType(Arg.second));
    }

    Type *RetType = getLLVMType(Proto->getReturnType());
    FunctionType *FT = FunctionType::get(RetType, ArgTypes, false);
    Function *F = Function::Create(FT, Function::ExternalLinkage, Proto->getName(), &Module);

    unsigned Idx = 0;
    for (auto &Arg : F->args()) {
        if (Idx < Proto->getArgs().size())
            Arg.setName(Proto->getArgs()[Idx++].first);
    }
    return F;
}

Function *FunctionGen::emitFunctionDef(FunctionDefAST *FuncAST, 
                                       const std::function<void(StmtAST*)> &StmtEmitter) {
    PrototypeAST *Proto = FuncAST->getProto();
    Function *TheFunction = Module.getFunction(Proto->getName());

    if (!TheFunction)
        TheFunction = emitPrototype(Proto);

    if (!TheFunction) return nullptr;
    if (!TheFunction->empty()) {
        fprintf(stderr, "Error: Function %s cannot be redefined.\n", Proto->getName().c_str());
        return nullptr;
    }

    BasicBlock *BB = BasicBlock::Create(Context, "entry", TheFunction);
    Builder.SetInsertPoint(BB);

    std::map<std::string, llvm::AllocaInst*> OldNamedValues = NamedValues;
    NamedValues.clear();
    
    createArgumentAllocas(TheFunction, Proto->getArgs());

    for (const auto &Stmt : FuncAST->getBody()) {
        StmtEmitter(Stmt.get());
    }

    if (!Builder.GetInsertBlock()->getTerminator()) {
        if (Proto->getReturnType() == "VOID")
            Builder.CreateRetVoid();
        else
            Builder.CreateRet(Constant::getNullValue(TheFunction->getReturnType()));
    }

    verifyFunction(*TheFunction);
    NamedValues = OldNamedValues;
    return TheFunction;
}

static Value *GenerateCall(llvm::Module &Module, llvm::IRBuilder<> &Builder, llvm::LLVMContext &Context,
                    const std::string &CalleeName, const std::vector<llvm::Value*> &Args) {
    Function *CalleeF = Module.getFunction(CalleeName);
    if (!CalleeF) {
        std::vector<Type*> ArgTypes;
        for (auto *Val : Args) ArgTypes.push_back(Val->getType());
        FunctionType *FT = FunctionType::get(Type::getInt64Ty(Context), ArgTypes, false);
        CalleeF = Function::Create(FT, Function::ExternalLinkage, CalleeName, &Module);
    }

    if (CalleeF->arg_size() != Args.size()) {
        fprintf(stderr, "Error: Incorrect # arguments passed to %s\n", CalleeName.c_str());
        return nullptr;
    }

    if (CalleeF->getReturnType()->isVoidTy()) {
        return Builder.CreateCall(CalleeF, Args);
    }

    return Builder.CreateCall(CalleeF, Args, "calltmp");
}

llvm::Value *FunctionGen::emitCallExpr(CallExprAST *Call, const std::vector<llvm::Value*> &Args) {
    return GenerateCall(Module, Builder, Context, Call->getCallee(), Args);
}

void FunctionGen::emitCallStmt(CallStmtAST *Call, const std::vector<llvm::Value*> &Args) {
    GenerateCall(Module, Builder, Context, Call->getCallee(), Args);
}

void FunctionGen::emitReturn(ReturnStmtAST *Ret, llvm::Value *RetVal) {
    if (RetVal) {
        Builder.CreateRet(RetVal);
    } else {
        Builder.CreateRetVoid();
    }
}
