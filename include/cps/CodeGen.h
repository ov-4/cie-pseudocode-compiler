#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "cps/AST.h"
#include "cps/RuntimeCheck.h"
#include <map>
#include <memory>

namespace cps {

class ArrayHandler;

class CodeGen {
    std::unique_ptr<llvm::LLVMContext> TheContext;
    std::unique_ptr<llvm::Module> TheModule;
    std::unique_ptr<llvm::IRBuilder<>> Builder;
    std::map<std::string, llvm::AllocaInst*> NamedValues;
    
    std::unique_ptr<ArrayHandler> Arrays;
    std::unique_ptr<RuntimeCheck> RuntimeChecker;

    llvm::FunctionCallee PrintfFunc;
    llvm::FunctionCallee ScanfFunc;
    llvm::Value *PrintfFormatStr;
    llvm::Value *ScanfFormatStr;

    void SetupExternalFunctions();
    llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction, const std::string &VarName);

    void emitIfStmt(IfStmtAST *Stmt);
    void emitWhileStmt(WhileStmtAST *Stmt);
    void emitRepeatStmt(RepeatStmtAST *Stmt);
    void emitForStmt(ForStmtAST *Stmt);

public:
    CodeGen();
    ~CodeGen();
    void compile(const std::vector<std::unique_ptr<StmtAST>> &Statements);
    void print();
    
    llvm::Value *emitExpr(ExprAST *Expr);
    void emitStmt(StmtAST *Stmt);
    
    friend class ArrayHandler;
};

}
