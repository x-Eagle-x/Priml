#ifndef _TOKEN_
#define _TOKEN_

#include <string>

enum ETokenKind {
    Illegal,
    Useless,
    At,

    /* Scope keywords */
    KwMacro,
    KwEnd,
    KwExtern,
    KwIntern,
    KwSub,
    KwEndsub,
    KwGosub,
    KwJumpsub,
    KwReturn,
    
    /* Stacking keywords */
    KwEmpty,
    KwDrop,
    KwSwap,
    KwDuplicate,
    
    /* Operator keywords */
    KwEqual,
    KwNotEqual,
    KwHigher,
    KwHigherEqual,
    KwLower,
    KwLowerEqual,
    
    /* Operators */
    OpEqual,
    OpAdd,
    OpSub,
    OpMul,
    OpDiv,

    /* Literals */
    StringLiteral,
    NumLiteral,
    
    /* Others */
    Identifier,
    Newline,
    EndOfFile
};

class CToken {
public:
    std::string m_Raw;
    ETokenKind m_Kind;

    CToken(std::string Raw, ETokenKind Kind) {
        m_Raw = Raw;
        m_Kind = Kind;
    }
};

#endif