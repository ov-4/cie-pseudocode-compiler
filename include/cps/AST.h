#pragma once
#include <string>
#include <memory>
#include <vector>

namespace cps {

class ExprAST {
public:
    virtual ~ExprAST() = default;
};

class NumberExprAST : public ExprAST {
    int Val;
public:
    NumberExprAST(int Val) : Val(Val) {}
    int getVal() const { return Val; }
};

class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string &Name) : Name(Name) {}
    const std::string &getName() const { return Name; }
};

class StmtAST {
public:
    virtual ~StmtAST() = default;
};

class DeclareStmtAST : public StmtAST {
    std::string Name;
public:
    DeclareStmtAST(const std::string &Name) : Name(Name) {}
    const std::string &getName() const { return Name; }
};

class AssignStmtAST : public StmtAST {
    std::string Name;
    std::unique_ptr<ExprAST> Expr;
public:
    AssignStmtAST(const std::string &Name, std::unique_ptr<ExprAST> Expr)
        : Name(Name), Expr(std::move(Expr)) {}
    const std::string &getName() const { return Name; }
    ExprAST *getExpr() const { return Expr.get(); }
};

class InputStmtAST : public StmtAST {
    std::string Name;
public:
    InputStmtAST(const std::string &Name) : Name(Name) {}
    const std::string &getName() const { return Name; }
};

class OutputStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> Expr;
public:
    OutputStmtAST(std::unique_ptr<ExprAST> Expr) : Expr(std::move(Expr)) {}
    ExprAST *getExpr() const { return Expr.get(); }
};

class BinaryExprAST : public ExprAST {
    int Op;
    std::unique_ptr<ExprAST> LHS, RHS;
public:
    BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
        : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
    int getOp() const { return Op; }
    ExprAST *getLHS() const { return LHS.get(); }
    ExprAST *getRHS() const { return RHS.get(); }
};

class IfStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> Cond;
    std::vector<std::unique_ptr<StmtAST>> ThenStmts;
    std::vector<std::unique_ptr<StmtAST>> ElseStmts;
public:
    IfStmtAST(std::unique_ptr<ExprAST> Cond,
              std::vector<std::unique_ptr<StmtAST>> ThenStmts,
              std::vector<std::unique_ptr<StmtAST>> ElseStmts)
        : Cond(std::move(Cond)), ThenStmts(std::move(ThenStmts)), ElseStmts(std::move(ElseStmts)) {}

    ExprAST *getCond() const { return Cond.get(); }
    const std::vector<std::unique_ptr<StmtAST>> &getThenStmts() const { return ThenStmts; }
    const std::vector<std::unique_ptr<StmtAST>> &getElseStmts() const { return ElseStmts; }
};


} // namespace cps
