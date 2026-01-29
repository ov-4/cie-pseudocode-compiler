#include "cps/Parser.h"

using namespace cps;

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
    if (CurTok == tok_number)
        return ParseNumberExpr();
    if (CurTok == tok_identifier)
        return ParseIdentifierExpr();
    return nullptr;
}

std::vector<std::unique_ptr<StmtAST>> Parser::Parse() {
    std::vector<std::unique_ptr<StmtAST>> Statements;

    while (CurTok != tok_eof) {
        if (CurTok == tok_declare) {
            // DECLARE <id> : INTEGER
            getNextToken(); // eat DECLARE
            if (CurTok != tok_identifier) break; // Error
            std::string Name = Lex.IdentifierStr;
            getNextToken(); // eat Name
            
            if (CurTok != tok_colon) break; // Error: expected ':'
            getNextToken(); // eat ':'

            if (CurTok != tok_integer_kw) break; // Error: expected INTEGER
            getNextToken(); // eat INTEGER

            Statements.push_back(std::make_unique<DeclareStmtAST>(Name));
        } 
        else if (CurTok == tok_identifier) {
            // <id> <- <expr>
            std::string Name = Lex.IdentifierStr;
            getNextToken(); // eat Name

            if (CurTok != tok_assign) break; // Error: expected '<-'
            getNextToken(); // eat '<-'

            auto Expr = ParseExpression();
            Statements.push_back(std::make_unique<AssignStmtAST>(Name, std::move(Expr)));
        }
        else if (CurTok == tok_input) {
            // INPUT <id>
            getNextToken(); // eat INPUT
            if (CurTok != tok_identifier) break;
            Statements.push_back(std::make_unique<InputStmtAST>(Lex.IdentifierStr));
            getNextToken();
        }
        else if (CurTok == tok_output) {
            // OUTPUT <expr>
            getNextToken(); // eat OUTPUT
            auto Expr = ParseExpression();
            Statements.push_back(std::make_unique<OutputStmtAST>(std::move(Expr)));
        }
        else {
            getNextToken(); // Skip unknown
        }
    }
    return Statements;
}
