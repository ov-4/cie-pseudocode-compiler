#pragma once
#include "cps/AST.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include <map>

namespace cps {

class CodeGen {
    std::unique_ptr<llvm::LLVMContext> TheContext;
    std::unique_ptr<llvm::Module> TheModule;
    std::unique_ptr<llvm::IRBuilder<>> Builder;
    std::map<std::string, llvm::AllocaInst*> NamedValues;

    llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction, 
                                            const std::string &VarName);

    llvm::FunctionCallee PrintfFunc;
    llvm::FunctionCallee ScanfFunc;
    llvm::Value *PrintfFormatStr;
    llvm::Value *ScanfFormatStr;
    llvm::Value *emitExpr(ExprAST *Expr);
    void emitStmt(StmtAST *Stmt);
    void emitIfStmt(IfStmtAST *Stmt);
    

    void SetupExternalFunctions();

public:
    CodeGen();
    void compile(const std::vector<std::unique_ptr<StmtAST>> &Statements);
    void print();
};

} // namespace cps
