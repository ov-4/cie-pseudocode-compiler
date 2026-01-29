#pragma once
#include "cps/Lexer.h"
#include "cps/AST.h"
#include <vector>
#include <memory>
#include <map>

namespace cps {

class Parser {
    Lexer &Lex;
    int CurTok;
    
    std::map<int, int> BinopPrecedence;

    int getNextToken();
    int GetTokPrecedence();

    std::unique_ptr<ExprAST> ParseExpression();
    std::unique_ptr<ExprAST> ParsePrimary();
    std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS); 

    std::unique_ptr<ExprAST> ParseNumberExpr();
    std::unique_ptr<ExprAST> ParseIdentifierExpr();
    std::unique_ptr<ExprAST> ParseParenExpr();

    std::unique_ptr<StmtAST> ParseStatement();
    std::unique_ptr<StmtAST> ParseIfStmt();
    std::unique_ptr<StmtAST> ParseWhileStmt();
    std::unique_ptr<StmtAST> ParseRepeatStmt();
    std::unique_ptr<StmtAST> ParseForStmt();
    
    std::unique_ptr<StmtAST> ParseDeclare();
    
public:
    Parser(Lexer &L);
    std::vector<std::unique_ptr<StmtAST>> Parse();
};

} // namespace cps
