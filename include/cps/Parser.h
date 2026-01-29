#pragma once
#include "cps/Lexer.h"
#include "cps/AST.h"
#include <vector>
#include <memory>

namespace cps {

class Parser {
    Lexer &Lex;
    int CurTok;

    int getNextToken();
    std::unique_ptr<ExprAST> ParseExpression();
    std::unique_ptr<ExprAST> ParseNumberExpr();
    std::unique_ptr<ExprAST> ParseParenExpr(); // 预留
    std::unique_ptr<ExprAST> ParseIdentifierExpr();

public:
    Parser(Lexer &L) : Lex(L) { getNextToken(); }
    
    std::vector<std::unique_ptr<StmtAST>> Parse();
};

} // namespace cps
