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
    tok_colon = -9,      // :

    tok_if = -10,
    tok_then = -11,
    tok_else = -12,
    tok_endif = -13,
    
    tok_while = -14,
    tok_do = -15,
    tok_endwhile = -16,
    tok_repeat = -17,
    tok_until = -18,
    tok_for = -19,
    tok_to = -24,
    tok_step = -25,
    tok_next = -26,

    tok_eq = -20, // =
    tok_ne = -21, // <>
    tok_le = -22, // <=
    tok_ge = -23  // >=
};

class Lexer {
public:
    std::string IdentifierStr;
    int NumVal;

    int gettok();
};

} // namespace cps
