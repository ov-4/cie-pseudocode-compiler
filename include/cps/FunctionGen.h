#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "cps/FunctionAST.h"
#include <map>
#include <vector>
#include <functional>
#include <string>
#include <tuple>

namespace cps {

class FunctionGen {
    llvm::LLVMContext &Context;
    llvm::Module &Module;
    llvm::IRBuilder<> &Builder;
    
    std::map<std::string, llvm::Value*> &NamedValues;

    void createArgumentAllocas(llvm::Function *F, const std::vector<std::tuple<std::string, std::string, bool>> &Args);

public:
    FunctionGen(llvm::LLVMContext &C, llvm::Module &M, llvm::IRBuilder<> &B, 
                std::map<std::string, llvm::Value*> &NV)
        : Context(C), Module(M), Builder(B), NamedValues(NV) {}

    llvm::Type *getLLVMType(const std::string &TypeName);

    llvm::Function *emitPrototype(PrototypeAST *Proto);

    llvm::Function *emitFunctionDef(FunctionDefAST *FuncAST, const std::function<void(StmtAST*)> &StmtEmitter);

    llvm::Value *emitCallExpr(CallExprAST *Call, const std::vector<llvm::Value*> &Args);
    
    void emitCallStmt(CallStmtAST *Call, const std::vector<llvm::Value*> &Args);

    void emitReturn(ReturnStmtAST *Ret, llvm::Value *RetVal);
};

}
