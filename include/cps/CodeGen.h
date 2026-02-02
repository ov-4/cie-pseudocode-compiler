#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "cps/AST.h"
#include "cps/RuntimeCheck.h"
#include "cps/FunctionGen.h"

#include "cps/IntegerHandler.h"
#include "cps/RealHandler.h"
#include "cps/BooleanHandler.h"
#include "cps/ArithmeticHandler.h"
#include "cps/StringHandler.h"

#include <map>
#include <memory>

namespace cps {

class ArrayHandler;

class CodeGen {
    std::unique_ptr<llvm::LLVMContext> TheContext;
    std::unique_ptr<llvm::Module> TheModule;
    std::unique_ptr<llvm::IRBuilder<>> Builder;
    std::map<std::string, llvm::Value*> NamedValues;
    
    std::unique_ptr<ArrayHandler> Arrays;
    std::unique_ptr<RuntimeCheck> RuntimeChecker;
    std::unique_ptr<FunctionGen> FuncGen;

    std::unique_ptr<IntegerHandler> IntHandler;
    std::unique_ptr<RealHandler> RealHelper;
    std::unique_ptr<BooleanHandler> BoolHandler;
    std::unique_ptr<ArithmeticHandler> ArithHandler;
    std::unique_ptr<StringHandler> StrHandler;

    llvm::FunctionCallee PrintfFunc;
    llvm::FunctionCallee ScanfFunc;
    
    llvm::Value *PrintfFormatStr;       // %lld\n
    llvm::Value *PrintfFloatFormatStr;  // %f\n
    llvm::Value *PrintfStringFormatStr; // %s\n
    
    llvm::Value *ScanfFormatStr;        // %lld
    llvm::Value *ScanfFloatFormatStr;   // %lf
    llvm::Value *ScanfStringFormatStr;  // %s
    
    llvm::Value *TrueStr;               // "TRUE"
    llvm::Value *FalseStr;              // "FALSE"

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
