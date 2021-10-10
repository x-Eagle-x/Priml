#include "lexer.hpp"
#include "token.hpp"

namespace NIdentMatcher {
    std::map<std::string, ETokenKind> Identifiers = {
        {"macro", KwMacro},
        {"end", KwEnd},
        {"extern", KwExtern},
        {"intern", KwIntern},
        {"sub", KwSub},
        {"endsub", KwEndsub},
        {"gosub", KwGosub},
        {"jumpsub", KwJumpsub},
        {"return", KwReturn},
        {"goto", KwGoto},
        
        {"empty", KwEmpty},
        {"drop", KwDrop},
        {"swap", KwSwap},
        {"dup", KwDuplicate},

        {"eq", KwEqual},
        {"neq", KwNotEqual},
        {"hi", KwHigher},
        {"hieq", KwHigherEqual},
        {"lw", KwLower},
        {"lweq", KwLowerEqual},

        // {"over", KwOver},
        // {"rot", KwRotate}
    };

    ETokenKind MatchIdent(std::string What);
};

ETokenKind NIdentMatcher::MatchIdent(std::string What) {
    if (Identifiers.find(What) != Identifiers.end()) {
        return Identifiers[What];
    }
    else {
        return ETokenKind::Identifier;
    }
}

bool isident(char What) {
    return isalnum(What) || What == '_';
}

CToken CLexer::GetToken() {
    ETokenKind Kind = ETokenKind::Illegal;
    char Char = CurrentChar();

    switch (Char) {
        case '+': Kind = ETokenKind::OpAdd; break;
        case '-': Kind = ETokenKind::OpSub; break;
        case '*': Kind = ETokenKind::OpMul; break;
        case '/': Kind = ETokenKind::OpDiv; break;

        case '('...')': Kind = ETokenKind::Useless; break;
        case '@': Kind = ETokenKind::At; break;

        case '0'...'9': {
            Kind = ETokenKind::NumLiteral;
            std::string RawNumber(1, Char);

            while (isalnum(TempPeekChar())) {
                if (isalpha(PeekChar())) {
                    Kind = ETokenKind::Illegal;
                }
                RawNumber.push_back(CurrentChar());
            }
            
            return TOKEN_STR(RawNumber, Kind);
        }

        case '`': {
            Kind = ETokenKind::StringLiteral;
            std::string StringLiteral;

            while (TempPeekChar() != '`' && TempPeekChar() != '\0') {
                StringLiteral.push_back(PeekChar());                
            }
            
            PeekChar();
            while (isident(TempPeekChar())) {
                Kind = ETokenKind::Illegal;
                StringLiteral.push_back(PeekChar());
            }
            
            return TOKEN_STR(StringLiteral, Kind);
        }

        case '_':
        case 'a'...'z':
        case 'A'...'Z': {
            std::string RawToken(1, Char);
            while (isident(TempPeekChar())) {
                RawToken.push_back(PeekChar());
            }

            Kind = NIdentMatcher::MatchIdent(RawToken);
            return TOKEN_STR(RawToken, Kind);
        }

        case '\n': Kind = ETokenKind::Newline; break;
        case EOF: Kind = ETokenKind::EndOfFile; break;
    }

    return TOKEN_CHAR(Char, Kind);
}

/*
    `string`x = illegal
    100x = illegal
    x100 = legal
*/

char CLexer::TempPeekChar(std::size_t Peek) {
    if (m_Index + Peek > m_Input.size()) {
        return '\0';
    }
    return m_Input[m_Index + Peek];
}

char CLexer::PeekChar(std::size_t Peek) {
    if (m_Index + Peek > m_Input.size()) {
        return '\0';
    }
    return m_Input[m_Index += Peek];
}

char CLexer::CurrentChar() {
    return m_Input[m_Index];
}

void CLexer::Lex() {
    for (m_Index = 0; m_Index < m_Input.size(); m_Index++) {
        char Char = CurrentChar();
        if (Char == '?') {
            while (TempPeekChar() != '?' && TempPeekChar() != '\n') {
                PeekChar();
            }
            PeekChar();
            continue;
        }
        else if (Char == ' ' || Char == '\t') {
            continue;
        }

        auto Token = GetToken();
        m_Tokens.push_back(Token);
    }
}

void CLexer::Feed(std::string Input) {
    m_Input += Input;
}