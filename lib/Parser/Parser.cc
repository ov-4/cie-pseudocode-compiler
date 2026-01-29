#include "cps/Parser.h"

using namespace cps;

Parser::Parser(Lexer &L) : Lex(L) {
    getNextToken();

    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40;
    BinopPrecedence['/'] = 40; 
}

int Parser::GetTokPrecedence() {
    if (!isascii(CurTok)) return -1;
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) return -1;
    return TokPrec;
}

std::unique_ptr<ExprAST> Parser::ParseParenExpr() {
    getNextToken();
    auto V = ParseExpression();
    if (!V) return nullptr;
    if (CurTok != ')') {
        fprintf(stderr, "Error: expected ')'\n");
        return nullptr;
    }
    getNextToken();
    return V;
}

std::unique_ptr<ExprAST> Parser::ParsePrimary() {
    switch (CurTok) {
    case tok_identifier: return ParseIdentifierExpr();
    case tok_number:     return ParseNumberExpr();
    case '(':            return ParseParenExpr();
    default:
        fprintf(stderr, "Error: unknown token when expecting an expression\n");
        return nullptr;
    }
}

std::unique_ptr<ExprAST> Parser::ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS) {
    while (true) {
        int TokPrec = GetTokPrecedence();

        if (TokPrec < ExprPrec)
            return LHS;

        int BinOp = CurTok;
        getNextToken();

        auto RHS = ParsePrimary();
        if (!RHS) return nullptr;

        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS) return nullptr;
        }

        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
    }
}


int Parser::getNextToken() {
    return CurTok = Lex.gettok();
}

std::unique_ptr<ExprAST> Parser::ParseNumberExpr() {
    auto Result = std::make_unique<NumberExprAST>(Lex.NumVal);
    getNextToken();
    return Result;
}

std::unique_ptr<ExprAST> Parser::ParseIdentifierExpr() {
    std::string IdName = Lex.IdentifierStr;
    getNextToken();
    return std::make_unique<VariableExprAST>(IdName);
}

std::unique_ptr<ExprAST> Parser::ParseExpression() {
    auto LHS = ParsePrimary();
    if (!LHS) return nullptr;
    return ParseBinOpRHS(0, std::move(LHS));
}

std::vector<std::unique_ptr<StmtAST>> Parser::Parse() {
    std::vector<std::unique_ptr<StmtAST>> Statements;

    while (CurTok != tok_eof) {
        if (CurTok == tok_declare) {
            // DECLARE <id> : INTEGER
            getNextToken();
            if (CurTok != tok_identifier) break; // Error
            std::string Name = Lex.IdentifierStr;
            getNextToken();
            
            if (CurTok != tok_colon) break; // Error
            getNextToken();

            if (CurTok != tok_integer_kw) break; // Error
            getNextToken();

            Statements.push_back(std::make_unique<DeclareStmtAST>(Name));
        } 
        else if (CurTok == tok_identifier) {
            // <id> <- <expr>
            std::string Name = Lex.IdentifierStr;
            getNextToken();

            if (CurTok != tok_assign) break; // Error
            getNextToken();

            auto Expr = ParseExpression();
            Statements.push_back(std::make_unique<AssignStmtAST>(Name, std::move(Expr)));
        }
        else if (CurTok == tok_input) {
            // INPUT <id>
            getNextToken();
            if (CurTok != tok_identifier) break;
            Statements.push_back(std::make_unique<InputStmtAST>(Lex.IdentifierStr));
            getNextToken();
        }
        else if (CurTok == tok_output) {
            // OUTPUT <expr>
            getNextToken();
            auto Expr = ParseExpression();
            Statements.push_back(std::make_unique<OutputStmtAST>(std::move(Expr)));
        }
        else {
            getNextToken();
        }
    }
    return Statements;
}
