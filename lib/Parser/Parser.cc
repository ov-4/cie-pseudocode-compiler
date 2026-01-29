#include "cps/Parser.h"

using namespace cps;

Parser::Parser(Lexer &L) : Lex(L) {
    getNextToken();

    BinopPrecedence[tok_eq] = 10;
    BinopPrecedence[tok_ne] = 10;
    BinopPrecedence[tok_le] = 10;
    BinopPrecedence[tok_ge] = 10;

    BinopPrecedence['<'] = 10;
    BinopPrecedence['>'] = 10;

    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40;
    BinopPrecedence['/'] = 40;
}

int Parser::GetTokPrecedence() {
    if (BinopPrecedence.find(CurTok) == BinopPrecedence.end()) return -1;
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) return -1;
    return TokPrec;
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
        if (TokPrec < ExprPrec) return LHS;

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

std::unique_ptr<ExprAST> Parser::ParseExpression() {
    auto LHS = ParsePrimary();
    if (!LHS) return nullptr;
    return ParseBinOpRHS(0, std::move(LHS));
}

std::vector<std::unique_ptr<StmtAST>> Parser::Parse() {
    std::vector<std::unique_ptr<StmtAST>> Statements;
    while (CurTok != tok_eof) {
        if (auto Stmt = ParseStatement()) {
            Statements.push_back(std::move(Stmt));
        } else {
            if (CurTok != tok_eof) getNextToken();
        }
    }
    return Statements;
}

std::unique_ptr<StmtAST> Parser::ParseIfStmt() {
    getNextToken();
    auto Cond = ParseExpression();
    if (!Cond) return nullptr;

    if (CurTok != tok_then) {
        fprintf(stderr, "Error: expected THEN\n");
        return nullptr;
    }
    getNextToken();

    std::vector<std::unique_ptr<StmtAST>> ThenStmts;
    while (CurTok != tok_else && CurTok != tok_endif && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) ThenStmts.push_back(std::move(Stmt));
    }

    std::vector<std::unique_ptr<StmtAST>> ElseStmts;
    if (CurTok == tok_else) {
        getNextToken();
        while (CurTok != tok_endif && CurTok != tok_eof) {
            auto Stmt = ParseStatement();
            if (Stmt) ElseStmts.push_back(std::move(Stmt));
        }
    }

    if (CurTok != tok_endif) {
        fprintf(stderr, "Error: expected ENDIF\n");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<IfStmtAST>(std::move(Cond), std::move(ThenStmts), std::move(ElseStmts));
}

std::unique_ptr<StmtAST> Parser::ParseWhileStmt() {
    getNextToken();
    auto Cond = ParseExpression();
    if (!Cond) return nullptr;

    if (CurTok != tok_do) {
        fprintf(stderr, "Error: expected DO after WHILE condition\n");
        return nullptr;
    }
    getNextToken();

    std::vector<std::unique_ptr<StmtAST>> Body;
    while (CurTok != tok_endwhile && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) Body.push_back(std::move(Stmt));
    }

    if (CurTok != tok_endwhile) {
        fprintf(stderr, "Error: expected ENDWHILE\n");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<WhileStmtAST>(std::move(Cond), std::move(Body));
}

std::unique_ptr<StmtAST> Parser::ParseRepeatStmt() {
    getNextToken();

    std::vector<std::unique_ptr<StmtAST>> Body;
    while (CurTok != tok_until && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) Body.push_back(std::move(Stmt));
    }

    if (CurTok != tok_until) {
        fprintf(stderr, "Error: expected UNTIL\n");
        return nullptr;
    }
    getNextToken();

    auto Cond = ParseExpression();
    if (!Cond) return nullptr;

    return std::make_unique<RepeatStmtAST>(std::move(Body), std::move(Cond));
}

std::unique_ptr<StmtAST> Parser::ParseForStmt() {
    getNextToken();
    if (CurTok != tok_identifier) {
        fprintf(stderr, "Error: expected identifier after FOR\n");
        return nullptr;
    }
    std::string VarName = Lex.IdentifierStr;
    getNextToken();

    if (CurTok != tok_assign) {
        fprintf(stderr, "Error: expected '<-' in FOR loop\n");
        return nullptr;
    }
    getNextToken();

    auto Start = ParseExpression();
    if (!Start) return nullptr;

    if (CurTok != tok_to) {
        fprintf(stderr, "Error: expected TO in FOR loop\n");
        return nullptr;
    }
    getNextToken();

    auto End = ParseExpression();
    if (!End) return nullptr;

    std::unique_ptr<ExprAST> Step = nullptr;
    if (CurTok == tok_step) {
        getNextToken();
        Step = ParseExpression();
    }

    std::vector<std::unique_ptr<StmtAST>> Body;
    while (CurTok != tok_next && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) Body.push_back(std::move(Stmt));
    }

    if (CurTok != tok_next) {
        fprintf(stderr, "Error: expected NEXT\n");
        return nullptr;
    }
    getNextToken();
    
    if (CurTok != tok_identifier) {
        fprintf(stderr, "Error: expected identifier after NEXT (e.g., NEXT %s)\n", VarName.c_str());
        return nullptr;
    }

    if (Lex.IdentifierStr != VarName) {
        fprintf(stderr, "Error: NEXT identifier '%s' does not match FOR variable '%s'\n", 
                Lex.IdentifierStr.c_str(), VarName.c_str());
        return nullptr;
    }
    getNextToken();

    return std::make_unique<ForStmtAST>(VarName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
}

std::unique_ptr<StmtAST> Parser::ParseStatement() {
    if (CurTok == tok_declare) {
        getNextToken();
        if (CurTok != tok_identifier) return nullptr;
        std::string Name = Lex.IdentifierStr;
        getNextToken();
        if (CurTok != tok_colon) return nullptr;
        getNextToken();
        if (CurTok != tok_integer_kw) return nullptr;
        getNextToken();
        return std::make_unique<DeclareStmtAST>(Name);
    }
    else if (CurTok == tok_identifier) {
        std::string Name = Lex.IdentifierStr;
        getNextToken();
        if (CurTok != tok_assign) return nullptr;
        getNextToken();
        auto Expr = ParseExpression();
        return std::make_unique<AssignStmtAST>(Name, std::move(Expr));
    }
    else if (CurTok == tok_input) {
        getNextToken();
        if (CurTok != tok_identifier) return nullptr;
        std::string Name = Lex.IdentifierStr;
        getNextToken();
        return std::make_unique<InputStmtAST>(Name);
    }
    else if (CurTok == tok_output) {
        getNextToken();
        auto Expr = ParseExpression();
        return std::make_unique<OutputStmtAST>(std::move(Expr));
    }
    else if (CurTok == tok_if) {
        return ParseIfStmt();
    }
    else if (CurTok == tok_while) {
        return ParseWhileStmt();
    }
    else if (CurTok == tok_repeat) {
        return ParseRepeatStmt();
    }
    else if (CurTok == tok_for) {
        return ParseForStmt();
    }

    return nullptr;
}
