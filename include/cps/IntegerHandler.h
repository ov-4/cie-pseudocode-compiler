#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include <map>
#include <string>

namespace cps {

class IntegerHandler {
    llvm::LLVMContext &Context;
    llvm::IRBuilder<> &Builder;
    llvm::Module &Module;
    std::map<std::string, llvm::Value*> &NamedValues;

public:
    IntegerHandler(llvm::LLVMContext &Ctx, llvm::IRBuilder<> &B, llvm::Module &M, std::map<std::string, llvm::Value*> &NV)
        : Context(Ctx), Builder(B), Module(M), NamedValues(NV) {}

    void emitDeclare(const std::string &Name);
    llvm::Value *createLiteral(int64_t Val);
};

}
