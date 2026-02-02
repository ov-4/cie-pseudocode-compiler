#include "cps/BooleanHandler.h"

using namespace llvm;
using namespace cps;

void BooleanHandler::emitDeclare(const std::string &Name) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    AllocaInst *Alloca = TmpB.CreateAlloca(Type::getInt1Ty(Context), nullptr, Name);
    Builder.CreateStore(ConstantInt::get(Context, APInt(1, 0)), Alloca);
    NamedValues[Name] = Alloca;
}

Value *BooleanHandler::createLiteral(bool Val) {
    return ConstantInt::get(Context, APInt(1, Val ? 1 : 0));
}
