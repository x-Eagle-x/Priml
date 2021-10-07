#include "parser.hpp"
#include "lexer.hpp"

#include <string>
#include <vector>
#include <algorithm>

#define STR(Str) #Str

#define INFO(Message, Extra) std::printf("\033[93mCompilation info\033[0m in %s @ \033[92m%i\033[0m: %s%s\n", m_InputFiles[m_CurrentFile].c_str(), m_Line + 1, Message, Extra)
#define WARN(Message, Extra) std::printf("\033[93mCompilation warning\033[0m in %s @ \033[92m%i\033[0m: %s%s\n", m_InputFiles[m_CurrentFile].c_str(), m_Line + 1, Message, Extra)
#define WARNG(Message, Extra) std::printf("\033[93mCompilation warning\033[0m: %s%s\n", Message, Extra)
#define ERROR(Message, Extra) std::fprintf(stderr, "\033[91mCompilation error\033[0m in %s @ \033[92m%i\033[0m: %s%s\n", m_InputFiles[m_CurrentFile].c_str(), m_Line + 1, Message, Extra); Fail()

#define EXPECT(Kind) if (TempPeekToken().m_Kind != Kind) {std::fprintf(stderr, "\033[91mCompilation error\033[0m in %s @ \033[92m%i\033[0m: expected \033[93m<%s>\033[0m, got <type:%i>.\n", m_InputFiles[m_CurrentFile].c_str(), m_Line + 1, STR(Kind), TempPeekToken().m_Kind); Fail();}

#define CHAR_TOKEN(x, y) CToken{std::string(1, x), y}
#define STR_TOKEN(x, y) CToken{x, y}

CToken CParser::PeekToken(std::size_t Peek) {
    if (m_Index + Peek > m_Lexer->m_Tokens.size()) {
        return STR_TOKEN("", ETokenKind::Illegal);
    }
    return m_Lexer->m_Tokens[m_Index += Peek];
}

CToken CParser::TempPeekToken(std::size_t Peek) {
    if (m_Index + Peek > m_Lexer->m_Tokens.size()) {
        return STR_TOKEN("", ETokenKind::Illegal);
    }
    return m_Lexer->m_Tokens[m_Index + Peek];
}

CToken CParser::CurrentToken() {
    return m_Lexer->m_Tokens[m_Index];
}

void CParser::Feed(CLexer &Lexer) {
    m_Lexer = &Lexer;
}

void CParser::OEmit(OP Opcode) {
    m_OutOverhead.push_back({Opcode, 0, 0, 0});
    m_WrittenBytes += 1;
}

void CParser::OEmitL(OP Opcode, uint32_t Left) {
    m_OutOverhead.push_back({Opcode, 1, Left, 0});
    m_WrittenBytes += 5;
}

void CParser::OEmitLR(OP Opcode, uint32_t Left, uint32_t Right) {
    m_OutOverhead.push_back({Opcode, 2, Left, Right});
    m_WrittenBytes += 9;
}

void CParser::WriteBytes(uint32_t Source) {
    uint32_byte Target = Source;
    m_WrittenBytes += 4;

    m_OutStream.put(Target[3]);
    m_OutStream.put(Target[2]);
    m_OutStream.put(Target[1]);
    m_OutStream.put(Target[0]);
}

void CParser::Emit(OP Opcode) {
    m_WrittenBytes += 1;
    m_OutStream.put(Opcode);
}

void CParser::EmitL(OP Opcode, uint32_t Left) {
    Emit(Opcode);
    WriteBytes(Left);
}

void CParser::EmitLR(OP Opcode, uint32_t Left, uint32_t Right) {
    Emit(Opcode);
    WriteBytes(Left);
    WriteBytes(Right);
}

void CParser::Fail() {
    m_FailedCompilation = true;
}

void CParser::AddFile(std::string Name) {
    m_InputFiles.push_back(Name);
}

void CParser::InitFile(std::string Output) {
    if (!m_OutStream.is_open()) {
        m_OutStream.open(m_OutFile = Output);
        if (!m_OutStream.is_open()) {
            ERROR("couldn't open output file ", m_OutFile.c_str());
            exit(1);
        }
    }
}

void CParser::Close() {
    m_OutStream.close();
}

void CParser::StartParsing() {
    for (m_Index = 0; m_Index < m_Lexer->m_Tokens.size(); m_Index++) {
        Parse();
    }

    if (m_SubSymbols.find("start") == m_SubSymbols.end()) {
        WARN("sub \"start\" not found", "");
        EmitL(OP::JMP, 1);
    }
    else {
        EmitL(OP::JMP, m_SubSymbols["start"]);
    }

    for (auto Inst : m_OutOverhead) {
        switch (Inst.m_Operands) {
            case 0: Emit(Inst.m_Opcode); break;
            case 1: EmitL(Inst.m_Opcode, Inst.m_Left); break;
            case 2: EmitLR(Inst.m_Opcode, Inst.m_Left, Inst.m_Right); break;
        }
    }

    if ((uint32_t) PROGRAM_SIZE < m_WrittenBytes) {
        WARNG("the maximum program size is not enough. Bugs may occur in the ReWave VM implementation.", "");
    }
    if (m_FailedCompilation) {
        std::remove(m_OutFile.c_str());
        //std::fprintf(stderr, "Compilation failed.\n");
    }
}

void CParser::Parse() {
    auto PushStack = [this](uint32_t What) {
        OEmitLR(OP::MOV, ++m_StackPtr, What);
        OEmitLR(OP::MOV, STACK_BASE_PTR, m_StackPtr);
    };

    auto PopStack = [this]() {
        OEmitLR(OP::MOV, m_StackPtr, 0);
        OEmitLR(OP::MOV, STACK_BASE_PTR, --m_StackPtr);
    };

    auto CheckCondition = [this](OP Condition) {
        OEmitLR(MEMOP(OP::CMP), m_StackPtr, m_StackPtr - 1);
        uint32_t CmpLocation = m_OutOverhead.size();
        OEmitL(Condition, 0);
        OEmitL(OP::JMP, 0);
        uint32_t Equals = m_WrittenBytes;
        while (TempPeekToken().m_Kind != ETokenKind::KwEnd) {
            if (TempPeekToken().m_Kind == ETokenKind::Illegal) {
                ERROR("no matching \"end\" found.", "");
                break;
            }
            PeekToken();
            Parse();
        }
        m_OutOverhead[CmpLocation].m_Left = Equals;
        m_OutOverhead[CmpLocation + 1].m_Left = m_WrittenBytes;
    };

    auto Token = CurrentToken();
    switch (Token.m_Kind) {
        case ETokenKind::KwMacro: {
            EXPECT(ETokenKind::Identifier);
            auto MacroName = PeekToken().m_Raw;
            if (m_Macros.find(MacroName) != m_Macros.end()) {
                ERROR("redefinition of macro ", MacroName.c_str());
            }
            while (TempPeekToken().m_Kind != ETokenKind::KwEnd && TempPeekToken().m_Kind != EndOfFile) {
                if (TempPeekToken().m_Raw == MacroName) {
                    ERROR("a macro cannot contain itself", "");
                    break;
                }
                m_Macros[MacroName].push_back(PeekToken());
            }
            PeekToken();
            break;
        }

        case ETokenKind::KwExtern: {
            uint32_t CallId;
            EXPECT(ETokenKind::Identifier);
            PeekToken();
            if (TempPeekToken().m_Raw == "$") {
                CallId = m_GlobalCallId++;
            }
            else {
                EXPECT(ETokenKind::NumLiteral);
                CallId = atoi(TempPeekToken().m_Raw.c_str());
            }
            m_ExternSymbols[CurrentToken().m_Raw] = CallId;
            PeekToken();
            break;
        }

        case ETokenKind::KwIntern: {
            EXPECT(ETokenKind::Identifier);
            m_SubSymbols[PeekToken().m_Raw] = 0;
            break;
        }

        case ETokenKind::KwSub: {
            if (m_CurrentSub != "") {
                ERROR("cannot do \"sub\" inside a sub region", "");
            }
            EXPECT(ETokenKind::Identifier);
            m_SubSymbols[m_CurrentSub = PeekToken().m_Raw] = m_WrittenBytes;
            break;
        }

        case ETokenKind::KwEndsub: {
            if (m_CurrentSub == "") {
                ERROR("cannot do \"endsub\" outside a sub region", "");
                break;
            }
            m_CurrentSub = "";
            OEmit(OP::RET);
            break;
        }
        
        case ETokenKind::KwGosub: {
            EXPECT(ETokenKind::Identifier);
            auto Sub = PeekToken().m_Raw;
            if (m_SubSymbols.find(Sub) == m_SubSymbols.end()) {
                ERROR("unknown sub ", Sub.c_str());
                break;
            }
            if (m_CurrentSub == Sub) {
                WARN("possible infinite loop", "");
            }
            OEmitL(OP::CALL, m_SubSymbols[Sub]);
            break;
        }
        
        case ETokenKind::KwJumpsub: {
            EXPECT(ETokenKind::Identifier);
            auto Sub = PeekToken().m_Raw;
            if (m_SubSymbols.find(Sub) == m_SubSymbols.end()) {
                ERROR("unknown sub ", Sub.c_str());
                break;
            }
            if (m_CurrentSub == Sub) {
                WARN("possible infinite loop", "");
            }
            OEmitL(OP::JMP, m_SubSymbols[Sub]);
            break;
        } 

        case ETokenKind::At: {
            EXPECT(ETokenKind::Identifier);
            auto LocName = PeekToken();
            if (m_Locations[m_CurrentSub].find(LocName.m_Raw) == m_Locations[m_CurrentSub].end()) {
                ERROR("already defined location ", LocName.m_Raw.c_str());
            }
            m_Locations[m_CurrentSub][LocName.m_Raw] = m_WrittenBytes;
            break;
        }

        case ETokenKind::KwReturn: {
            OEmit(OP::RET);
        }

        /* To be finished
        case ETokenKind::KwGoto: {
            EXPECT(ETokenKind::Identifier);
            break;
        }
        */

        case ETokenKind::KwEmpty: {
            while (m_StackPtr > m_StackStart) {
                OEmitLR(OP::MOV, m_StackPtr--, 0);
            }
            OEmitLR(OP::MOV, STACK_BASE_PTR, m_StackStart);
            break;
        }

        case ETokenKind::KwDrop: {
            PopStack();
            break;
        }

        case ETokenKind::KwDuplicate: {
            PushStack(0);
            OEmitLR(MEMOP(OP::MOV), m_StackPtr, m_StackPtr - 1);
            break;
        }

        case ETokenKind::KwSwap: {
            OEmitLR(MEMOP(OP::MOV), 0x05, m_StackPtr - 1);
            OEmitLR(MEMOP(OP::MOV), m_StackPtr - 1, m_StackPtr);
            OEmitLR(MEMOP(OP::MOV), m_StackPtr, 0x05);
            break;
        }

        case ETokenKind::KwEqual: {
            CheckCondition(OP::JEQ);
            break;
        }

        case ETokenKind::KwNotEqual: {
            CheckCondition(OP::JNQ);
            break;
        }

        case ETokenKind::KwHigher: {
            CheckCondition(OP::JHI);
            break;
        }

        case ETokenKind::KwHigherEqual: {
            CheckCondition(OP::JHE);
            break;
        }
        case ETokenKind::KwLower: {
            CheckCondition(OP::JLW);
            break;
        }

        case ETokenKind::KwLowerEqual: {
            CheckCondition(OP::JLE);
            break;
        }

        case ETokenKind::NumLiteral: {
            PushStack(atoi(Token.m_Raw.c_str()));
            break;
        }

        case ETokenKind::OpAdd: {
            OEmitLR(MEMOP(OP::ADD), m_StackPtr - 1, m_StackPtr);
            PopStack();
            break;
        }

        case ETokenKind::OpSub: {
            OEmitLR(MEMOP(OP::SUB), m_StackPtr - 1, m_StackPtr);
            PopStack();
            break;
        }

        case ETokenKind::OpMul: {
            OEmitLR(MEMOP(OP::MUL), m_StackPtr - 1, m_StackPtr);
            PopStack();
            break;
        }

        case ETokenKind::OpDiv: {
            OEmitLR(MEMOP(OP::DIV), m_StackPtr - 1, m_StackPtr);
            PopStack();
            break;
        }

        case ETokenKind::StringLiteral: {
            uint32_t StrLen = Token.m_Raw.size() + 1, StrLoc = m_WrittenBytes + 5;
            OEmitL(OP::JMP, m_WrittenBytes + 5 + StrLen);
            for (auto Letter : Token.m_Raw) {
                OEmit((OP) Letter);
            }
            OEmit((OP) '\0');
            PushStack(StrLoc);    
            break;
        }

        case ETokenKind::Identifier: {
            if (m_Macros.find(Token.m_Raw) != m_Macros.end()) {
                std::reverse(m_Macros[Token.m_Raw].begin(), m_Macros[Token.m_Raw].end());
                m_Lexer->m_Tokens.erase(m_Lexer->m_Tokens.begin() + m_Index);
                for (auto MacroToken : m_Macros[Token.m_Raw]) {
                    m_Lexer->m_Tokens.insert(m_Lexer->m_Tokens.begin() + m_Index, MacroToken);
                }
                m_Index--;
                break;
            }
            else if (m_ExternSymbols.find(Token.m_Raw) == m_ExternSymbols.end()) {
                ERROR("unknown symbol ", Token.m_Raw.c_str());
                break;
            }
            OEmitL(OP::INT, m_ExternSymbols[Token.m_Raw]);
            break;
        }

        case ETokenKind::Newline: {
            m_Line++;
            break;
        }

        case ETokenKind::EndOfFile: {
            m_CurrentFile++;
            m_Line = 0;
        }
    }
}