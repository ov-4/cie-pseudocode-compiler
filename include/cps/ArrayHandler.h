#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "cps/AST.h"
#include "cps/RuntimeCheck.h"
#include <map>
#include <string>
#include <vector>

namespace cps {

struct ArrayMetadata {
    int Rank;
    llvm::Type* ElementType;
    std::vector<llvm::Value*> LowerBounds;
    std::vector<llvm::Value*> UpperBounds;
    std::vector<llvm::Value*> Multipliers; 
};

class CodeGen;

class ArrayHandler {
    llvm::LLVMContext *TheContext;
    llvm::IRBuilder<> *Builder;
    llvm::Module *TheModule;
    std::map<std::string, llvm::AllocaInst*> *NamedValues;

    std::map<std::string, ArrayMetadata> ArrayTable;
    RuntimeCheck &RuntimeChecker;

    llvm::FunctionCallee MallocFunc;
    llvm::FunctionCallee FreeFunc;

    llvm::Value* computeFlatIndex(const std::string &Name, const std::vector<llvm::Value*> &Indices);
    
    void emitPrintLoop(const std::string &Name, int CurrentDim, std::vector<llvm::Value*> CurrentIndices, llvm::FunctionCallee PrintfFunc, llvm::Value* FmtStr);

public:
    ArrayHandler(llvm::LLVMContext &C, llvm::IRBuilder<> &B, llvm::Module &M, std::map<std::string, llvm::AllocaInst*> &NV);

    void setupExternalFunctions();
    
    void emitArrayDeclare(ArrayDeclareStmtAST *Stmt, CodeGen &CG);
    void emitArrayAssign(ArrayAssignStmtAST *Stmt, CodeGen &CG);
    ArrayHandler(llvm::LLVMContext &C, llvm::IRBuilder<> &B, llvm::Module &M, std::map<std::string, llvm::AllocaInst*> &NV, RuntimeCheck &RC);
    
    llvm::Value* emitArrayAccess(ArrayAccessExprAST *Expr, CodeGen &CG);

    bool tryEmitArrayOutput(ExprAST *Expr, CodeGen &CG, llvm::FunctionCallee PrintfFunc, llvm::Value* FmtStr);
};

}
