#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

namespace cps {

class RuntimeCheck {
    llvm::Module &TheModule;
    llvm::LLVMContext &TheContext;
    llvm::IRBuilder<> &Builder;

    llvm::FunctionCallee PrintfFunc;
    llvm::FunctionCallee ExitFunc;
    llvm::Value *DivZeroMsg;
    llvm::Value *OutOfBoundsMsg;

    void setupExternalFunctions();
    void emitErrorAndExit(llvm::Value *Condition, llvm::Value *Msg, int Line);

public:
    RuntimeCheck(llvm::Module &M, llvm::LLVMContext &C, llvm::IRBuilder<> &B);

    void emitDivZeroCheck(llvm::Value *Divisor, int Line);
    void emitIndexCheck(llvm::Value *Index, llvm::Value *Lower, llvm::Value *Upper, int Line);
};

}
