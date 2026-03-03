#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

namespace cps {

class CharHandler {
    llvm::LLVMContext &Context;
    llvm::IRBuilder<> &Builder;
    llvm::Module &Module;

public:
    CharHandler(llvm::LLVMContext &Ctx, llvm::IRBuilder<> &B, llvm::Module &M);
    
    // ASC(ThisChar: CHAR) RETURNS INTEGER
    llvm::Value *emitAsc(llvm::Value *CharVal);
    
    // CHR(x: INTEGER) RETURNS CHAR
    llvm::Value *emitChr(llvm::Value *IntVal);
};

} // namespace cps
