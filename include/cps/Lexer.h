#pragma once
#include <string>
#include <iostream>

namespace cps {

enum Token {
    tok_eof = -1,
    tok_declare = -2,
    tok_integer_kw = -3, // INTEGER
    tok_input = -4,
    tok_output = -5,
    tok_identifier = -6,
    tok_number = -7,
    tok_assign = -8,     // <-
    tok_colon = -9       // :
};

class Lexer {
public:
    std::string IdentifierStr;
    int NumVal;

    int gettok();
};

} // namespace cps
