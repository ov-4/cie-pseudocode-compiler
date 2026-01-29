#include "cps/Lexer.h"
#include <cctype>
#include <cstdio>

using namespace cps;

int Lexer::gettok() {
    static int LastChar = ' ';

    while (isspace(LastChar))
        LastChar = getchar();

    if (isalpha(LastChar)) {
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getchar())) || LastChar == '_')
            IdentifierStr += LastChar;

        if (IdentifierStr == "DECLARE") return tok_declare;
        if (IdentifierStr == "INTEGER") return tok_integer_kw;
        if (IdentifierStr == "INPUT") return tok_input;
        if (IdentifierStr == "OUTPUT") return tok_output;
        return tok_identifier;
    }

    if (isdigit(LastChar)) {
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar));
        NumVal = strtol(NumStr.c_str(), nullptr, 10);
        return tok_number;
    }

    if (LastChar == '<') {
        LastChar = getchar();
        if (LastChar == '-') {
            LastChar = getchar();
            return tok_assign;
        }
        return '<';
    }

    // for comments
    
    if (LastChar == ':') {
        LastChar = getchar();
        return tok_colon;
    }

    if (LastChar == EOF)
        return tok_eof;

    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}
