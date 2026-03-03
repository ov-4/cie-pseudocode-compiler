#include "cps/CharHandler.h"

using namespace llvm;
using namespace cps;

CharHandler::CharHandler(LLVMContext &Ctx, IRBuilder<> &B, llvm::Module &M)
    : Context(Ctx), Builder(B), Module(M) {}

Value *CharHandler::emitAsc(Value *CharVal) {
    return Builder.CreateZExt(CharVal, Type::getInt32Ty(Context), "asc_val");
}

Value *CharHandler::emitChr(Value *IntVal) {
    return Builder.CreateTrunc(IntVal, Type::getInt8Ty(Context), "chr_val");
}
