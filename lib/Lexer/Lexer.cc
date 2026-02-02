#include "cps/Lexer.h"
#include <cctype>
#include <cstdio>
#include <string>
#include <algorithm>
#include <cstdlib>

using namespace cps;

int Lexer::gettok() {
    static int LastChar = ' ';

    while (isspace(LastChar)) {
        if (LastChar == '\n') CurrentLine++;
        LastChar = getchar();
    }

    if (isalpha(LastChar)) {
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getchar())) || LastChar == '_')
            IdentifierStr += LastChar;

        if (IdentifierStr == "DECLARE") return tok_declare;
        if (IdentifierStr == "INTEGER") return tok_integer_kw;
        if (IdentifierStr == "BOOLEAN") return tok_boolean_kw; 
        if (IdentifierStr == "REAL")    return tok_real_kw; 
        
        if (IdentifierStr == "TRUE") return tok_true;
        if (IdentifierStr == "FALSE") return tok_false;

        if (IdentifierStr == "INPUT") return tok_input;
        if (IdentifierStr == "OUTPUT") return tok_output;
        
        if (IdentifierStr == "IF") return tok_if;
        if (IdentifierStr == "THEN") return tok_then;
        if (IdentifierStr == "ELSE") return tok_else;
        if (IdentifierStr == "ENDIF") return tok_endif;

        if (IdentifierStr == "WHILE") return tok_while;
        if (IdentifierStr == "DO") return tok_do;
        if (IdentifierStr == "ENDWHILE") return tok_endwhile;
        
        if (IdentifierStr == "REPEAT") return tok_repeat;
        if (IdentifierStr == "UNTIL") return tok_until;
        
        if (IdentifierStr == "FOR") return tok_for;
        if (IdentifierStr == "TO") return tok_to;
        if (IdentifierStr == "STEP") return tok_step;
        if (IdentifierStr == "NEXT") return tok_next;

        if (IdentifierStr == "ARRAY") return tok_array;
        if (IdentifierStr == "OF") return tok_of;

        if (IdentifierStr == "DIV") return tok_div;
        if (IdentifierStr == "MOD") return tok_mod;

        if (IdentifierStr == "FUNCTION") return tok_function;
        if (IdentifierStr == "ENDFUNCTION") return tok_endfunction;
        if (IdentifierStr == "PROCEDURE") return tok_procedure;
        if (IdentifierStr == "ENDPROCEDURE") return tok_endprocedure;
        if (IdentifierStr == "RETURN") return tok_return;
        if (IdentifierStr == "RETURNS") return tok_returns;
        if (IdentifierStr == "CALL") return tok_call;
        
        if (IdentifierStr == "BYREF") return tok_byref;
        if (IdentifierStr == "BYVAL") return tok_byval;

        return tok_identifier;
    }

    if (isdigit(LastChar)) {
        std::string NumStr;
        bool isReal = false;
        do {
            NumStr += LastChar;
            LastChar = getchar();
            if (LastChar == '.' && !isReal) {
                isReal = true;
                NumStr += '.';
                LastChar = getchar();
            }
        } while (isdigit(LastChar));
        
        if (isReal) {
            RealVal = strtod(NumStr.c_str(), nullptr);
            return tok_number_real;
        } else {
            NumVal = strtoll(NumStr.c_str(), nullptr, 10);
            return tok_number_int;
        }
    }

    if (LastChar == '<') {
        LastChar = getchar();
        if (LastChar == '-') { // <-
            LastChar = getchar();
            return tok_assign;
        }
        if (LastChar == '=') { // <=
            LastChar = getchar();
            return tok_le;
        }
        if (LastChar == '>') { // <>
            LastChar = getchar();
            return tok_ne;
        }
        return '<';
    }

    if (LastChar == '>') {
        LastChar = getchar();
        if (LastChar == '=') { // >=
            LastChar = getchar();
            return tok_ge;
        }
        return '>';
    }

    if (LastChar == '=') {
        LastChar = getchar();
        return tok_eq;
    }

    if (LastChar == ':') {
        LastChar = getchar();
        return tok_colon;
    }

    if (LastChar == '(') { LastChar = getchar(); return '('; }
    if (LastChar == ')') { LastChar = getchar(); return ')'; }
    if (LastChar == '[') { LastChar = getchar(); return '['; }
    if (LastChar == ']') { LastChar = getchar(); return ']'; }
    if (LastChar == ',') { LastChar = getchar(); return ','; }
    if (LastChar == '+') { LastChar = getchar(); return '+'; }
    if (LastChar == '-') { LastChar = getchar(); return '-'; }
    if (LastChar == '*') { LastChar = getchar(); return '*'; }

    if (LastChar == '/') {
        int NextChar = getchar();
        if (NextChar == '/') {
            do {
                LastChar = getchar();
            } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
            
            if (LastChar != EOF)
                return gettok();
        } else {
            LastChar = NextChar;
            return '/';
        }
    }

    if (LastChar == EOF)
        return tok_eof;

    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}
