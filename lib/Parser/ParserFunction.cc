#include "cps/Parser.h"
#include "cps/FunctionAST.h"
#include <tuple>

using namespace cps;

std::vector<std::tuple<std::string, std::string, bool>> Parser::ParsePrototypeArgs() {
    std::vector<std::tuple<std::string, std::string, bool>> Args;
    if (CurTok != '(') return Args;
    getNextToken();

    while (CurTok != ')') {
        if (CurTok == -1000 || CurTok == -1001) getNextToken();

        bool IsRef = false;
        if (CurTok == tok_byref) {
            IsRef = true;
            getNextToken();
        } else if (CurTok == tok_byval) {
            IsRef = false;
            getNextToken();
        }

        if (CurTok != tok_identifier) {
            fprintf(stderr, "Error: Expected argument name\n");
            return Args;
        }
        std::string Name = Lex.IdentifierStr;
        getNextToken();

        if (CurTok != tok_colon) {
            fprintf(stderr, "Error: Expected ':' after argument name\n");
            return Args;
        }
        getNextToken();

        std::string Type = "INTEGER";
        if (CurTok == tok_integer_kw) {
            getNextToken();
        } else if (CurTok == tok_identifier) {
            Type = Lex.IdentifierStr; 
            getNextToken();
        } else {
             fprintf(stderr, "Error: Expected type (e.g. INTEGER) for argument '%s'\n", Name.c_str());
             return Args;
        }

        Args.emplace_back(Name, Type, IsRef);

        if (CurTok == ')') break;
        if (CurTok != ',') {
            fprintf(stderr, "Error: Expected ',' or ')'\n");
            return Args;
        }
        getNextToken();
    }
    getNextToken();
    return Args;
}


std::unique_ptr<StmtAST> Parser::ParseFunction() {
    getNextToken();
    std::string Name = Lex.IdentifierStr;
    getNextToken();

    auto Args = ParsePrototypeArgs();

    std::string RetType = "INTEGER";
    if (CurTok == tok_returns) { 
        getNextToken();
        getNextToken();
    }

    auto Proto = std::make_unique<PrototypeAST>(Name, std::move(Args), RetType);

    std::vector<std::unique_ptr<StmtAST>> Body;
    while (CurTok != tok_endfunction && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) Body.push_back(std::move(Stmt));
        else if (CurTok != tok_eof && CurTok != tok_endfunction) getNextToken();
    }
    getNextToken();

    return std::make_unique<FunctionDefAST>(std::move(Proto), std::move(Body));
}

std::unique_ptr<StmtAST> Parser::ParseProcedure() {
    getNextToken();
    std::string Name = Lex.IdentifierStr;
    getNextToken();

    auto Args = ParsePrototypeArgs();
    auto Proto = std::make_unique<PrototypeAST>(Name, std::move(Args), "VOID");

    std::vector<std::unique_ptr<StmtAST>> Body;
    while (CurTok != tok_endprocedure && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) Body.push_back(std::move(Stmt));
        else if (CurTok != tok_eof && CurTok != tok_endprocedure) getNextToken();
    }
    getNextToken();

    return std::make_unique<FunctionDefAST>(std::move(Proto), std::move(Body));
}

std::unique_ptr<StmtAST> Parser::ParseCallStmt() {
    getNextToken();
    std::string Callee = Lex.IdentifierStr;
    getNextToken();

    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok == '(') {
        getNextToken();
        while (CurTok != ')') {
            auto Arg = ParseExpression();
            if (Arg) Args.push_back(std::move(Arg));
            
            if (CurTok == ')') break;
            if (CurTok != ',') {
                fprintf(stderr, "Error: Expected ',' or ')' in call\n");
                return nullptr;
            }
            getNextToken();
        }
        getNextToken();
    }
    
    return std::make_unique<CallStmtAST>(Callee, std::move(Args));
}

std::unique_ptr<StmtAST> Parser::ParseReturnStmt() {
    getNextToken();
    std::unique_ptr<ExprAST> Expr = nullptr;
    if (CurTok != tok_endif && CurTok != tok_else && CurTok != tok_endfunction && CurTok != tok_endprocedure) {
        Expr = ParseExpression();
    }
    return std::make_unique<ReturnStmtAST>(std::move(Expr));
}
