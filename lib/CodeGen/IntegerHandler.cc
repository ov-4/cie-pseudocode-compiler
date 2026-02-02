#include "cps/IntegerHandler.h"

using namespace llvm;
using namespace cps;

void IntegerHandler::emitDeclare(const std::string &Name) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    AllocaInst *Alloca = TmpB.CreateAlloca(Type::getInt64Ty(Context), nullptr, Name);
    Builder.CreateStore(ConstantInt::get(Context, APInt(64, 0)), Alloca);
    NamedValues[Name] = Alloca;
}

Value *IntegerHandler::createLiteral(int64_t Val) {
    return ConstantInt::get(Context, APInt(64, Val, true));
}
