#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "cps/Lexer.h"

namespace cps {

class ArithmeticHandler {
    llvm::LLVMContext &Context;
    llvm::IRBuilder<> &Builder;

public:
    ArithmeticHandler(llvm::LLVMContext &Ctx, llvm::IRBuilder<> &B)
        : Context(Ctx), Builder(B) {}

    llvm::Value *emitBinaryOp(int Op, llvm::Value *LHS, llvm::Value *RHS, int Line);
};

}
