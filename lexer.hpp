#ifndef _LEXER_
#define _LEXER_

#include <vector>
#include <map>
#include <set>

#include "token.hpp"
#include "token.hpp"

class CLexer {
    std::size_t m_Index;
    std::string m_Input;

public:
    std::vector<CToken> m_Tokens;
    CToken GetToken();

    char TempPeekChar(std::size_t Peek = 1);
    char PeekChar(std::size_t Peek = 1);
    char CurrentChar();

    void Lex();
    void Feed(std::string Input);
};

#define TOKEN_CHAR(x, y) CToken{std::string(1, x), y}
#define TOKEN_STR(x, y) CToken{x, y}

#endif