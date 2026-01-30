#pragma once
#include <string>
#include <cstdint>

namespace cps {

enum Token {
    tok_eof = -1,

    tok_declare = -2,
    tok_integer_kw = -3,
    tok_input = -4,
    tok_output = -5,
    
    tok_if = -6,
    tok_then = -7,
    tok_else = -8,
    tok_endif = -9,
    
    tok_while = -10,
    tok_do = -11,
    tok_endwhile = -12,
    
    tok_repeat = -13,
    tok_until = -14,
    
    tok_for = -15,
    tok_to = -16,
    tok_step = -17,
    tok_next = -18,

    tok_array = -19,
    tok_of = -20,

    tok_identifier = -100,
    tok_number = -101,
    
    tok_assign = -200, // <-
    tok_eq = -201, // =
    tok_ne = -202, // <>
    tok_le = -203, // <=
    tok_ge = -204, // >=
    
    tok_colon = -300 // :
};

class Lexer {
    int CurrentLine = 1;
public:
    std::string IdentifierStr;
    int64_t NumVal;

    int gettok();
    int getLine() const { return CurrentLine; }
};

}
