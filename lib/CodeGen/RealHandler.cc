#include "cps/RealHandler.h"

using namespace llvm;
using namespace cps;

void RealHandler::emitDeclare(const std::string &Name) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    AllocaInst *Alloca = TmpB.CreateAlloca(Type::getDoubleTy(Context), nullptr, Name);
    Builder.CreateStore(ConstantFP::get(Context, APFloat(0.0)), Alloca);
    NamedValues[Name] = Alloca;
}

Value *RealHandler::createLiteral(double Val) {
    return ConstantFP::get(Context, APFloat(Val));
}
