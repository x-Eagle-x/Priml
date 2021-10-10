#include "parser.hpp"
#include "lexer.hpp"
#include "registers.hpp"

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

#define STR(Str) #Str

#define INFO(Message, Extra) std::printf("\033[93mCompilation info\033[0m in %s @ L\033[92m%i\033[0m: %s\033[96m%s\033[0m\n", m_InputFiles[m_CurrentFile].c_str(), m_Line + 1, Message, Extra)
#define WARN(Message, Extra) std::printf("\033[93mCompilation warning\033[0m in %s @ L\033[92m%i\033[0m: %s\033[96m%s\033[0m\n", m_InputFiles[m_CurrentFile].c_str(), m_Line + 1, Message, Extra)
#define WARNG(Message, Extra) std::printf("\033[93mCompilation warning\033[0m: %s\033[96m%s\033[0m\n", Message, Extra)
#define ERROR(Message, Extra) std::fprintf(stderr, "\033[91mCompilation error\033[0m in %s @ L\033[92m%i\033[0m: %s\033[96m%s\033[0m\n", m_InputFiles[m_CurrentFile].c_str(), m_Line + 1, Message, Extra); Fail()
#define ERRORG(Message, Extra) std::fprintf(stderr, "\033[91mCompilation error\033[0m: %s\033[96m%s\033[0m\n", Message, Extra); Fail()

#define EXPECT(Kind) if (TempPeekToken().m_Kind != Kind) {std::fprintf(stderr, "\033[91mCompilation error\033[0m in %s @ L\033[92m%i\033[0m: expected \033[93m<%s>\033[0m, got <type:%i>.\n", m_InputFiles[m_CurrentFile].c_str(), m_Line + 1, STR(Kind), TempPeekToken().m_Kind); Fail();}

#define CHAR_TOKEN(x, y) CToken{std::string(1, x), y}
#define STR_TOKEN(x, y) CToken{x, y}

enum OP_EX
{
    GOTO = 30
};

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

void CParser::OEmit(OP Opcode, std::string Extra) {
    m_OutOverhead.push_back({Opcode, 0, 0, 0, Extra});
    m_WrittenBytes += 1;
}

void CParser::OEmitL(OP Opcode, uint32_t Left, std::string Extra) {
    m_OutOverhead.push_back({Opcode, 1, Left, 0, Extra});
    m_WrittenBytes += 5;
}

void CParser::OEmitLR(OP Opcode, uint32_t Left, uint32_t Right, std::string Extra) {
    m_OutOverhead.push_back({Opcode, 2, Left, Right, Extra});
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

void CParser::SetNoStart() {
    m_NoStart = true;
}

void CParser::StartParsing() {
    EmitLR(OP::MOV, STACK_PTR, STACK_BASE);

    for (m_Index = 0; m_Index < m_Lexer->m_Tokens.size(); m_Index++) {
        Parse();
    }

    if (!m_NoStart) {
        if (m_SubSymbols.find("start") == m_SubSymbols.end()) {
            WARN("sub \"start\" not found", "");
            EmitL(OP::JMP, 1);
        }
        else {
            EmitL(OP::JMP, m_SubSymbols["start"]);
        }
    }

    for (auto Inst : m_OutOverhead) {
        if (Inst.m_Opcode == (OP) OP_EX::GOTO) {
            auto Delim = Inst.m_Extra.find(':');
            std::string Sub = Inst.m_Extra.substr(0, Delim), Location = &Inst.m_Extra[Delim + 1];
            if (m_Locations[Sub].find(Location) == m_Locations[Sub].end()) {
                ERRORG("can't do \"goto\" to the undefined location ", Inst.m_Extra.c_str());
            }
            Inst.m_Opcode = OP::JMP;
            Inst.m_Left = m_Locations[Sub][Location];
        }

        switch (Inst.m_Operands) {
            case 0: Emit(Inst.m_Opcode); break;
            case 1: EmitL(Inst.m_Opcode, Inst.m_Left); break;
            case 2: EmitLR(Inst.m_Opcode, Inst.m_Left, Inst.m_Right); break;
        }
    }

    if (PROGRAM_SIZE < m_WrittenBytes) {
        WARNG("the maximum program size is not enough. Bugs may occur in the ReWave VM implementation.", "");
    }
    if (m_FailedCompilation) {
        std::remove(m_OutFile.c_str());
        //std::fprintf(stderr, "Compilation failed.\n");
    }
}

void CParser::Parse() {
    auto PushStack = [this](uint32_t What) {
        OEmitLR(OP::ADD, STACK_PTR, 1);          // add stack, 1
        OEmitLR(MEML(OP::MOV), STACK_PTR, What); // mov %stack, what
    };

    auto PopStack = [this]() {
        //OEmitLR(MEML(OP::MOV), STACK_PTR, 0);
        OEmitLR(OP::SUB, STACK_PTR, 1);
    };

    auto CheckCondition = [this](OP Condition) {
        OEmitLR(OP::SUB, STACK_PTR, 1);
        OEmitLR(MEMR(OP::LOAD), REG::B0, STACK_PTR);
        OEmitLR(OP::ADD, STACK_PTR, 1);
        OEmitLR(MEMR(OP::LOAD), REG::A0, STACK_PTR);
        uint32_t CmpLocation = m_OutOverhead.size();
        OEmitL(Condition, 0);
        OEmitL(OP::JMP, 0);
        uint32_t Equals = m_WrittenBytes;
        while (TempPeekToken().m_Kind != ETokenKind::KwEnd) {
            if (TempPeekToken().m_Kind == ETokenKind::Illegal) {
                ERROR("no matching \"end\" found", "");
                break;
            }
            PeekToken();
            Parse();
        }
        m_OutOverhead[CmpLocation].m_Left = Equals;
        m_OutOverhead[CmpLocation + 1].m_Left = m_WrittenBytes;
        OEmitLR(OP::MOV, REG::CX, 0);
    };

    auto DoOperator = [this, PopStack](OP Operator) {
        OEmitLR(MEMR(OP::LOAD), REG::BX, STACK_PTR);
        OEmitLR(OP::SUB, STACK_PTR, 1);
        OEmitLR(MEMR(OP::LOAD), REG::CX, STACK_PTR);
        OEmitLR(MEMR(Operator), REG::CX, REG::BX);
        OEmitLR(MEMLR(OP::MOV), STACK_PTR, REG::CX);
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
            if (m_SubSymbols.find(TempPeekToken().m_Raw) != m_SubSymbols.end()) {
                ERROR("redefined sub region ", TempPeekToken().m_Raw.c_str());
            }
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
                //ERROR("unknown sub ", Sub.c_str());
                break;
            }
            if (m_CurrentSub == Sub) {
                WARN("possible infinite loop", "");
            }
            OEmitL(OP::JMP, m_SubSymbols[Sub], Sub);
            break;
        } 

        case ETokenKind::At: {
            EXPECT(ETokenKind::Identifier);
            auto LocName = PeekToken().m_Raw;
            if (m_Locations[m_CurrentSub].find(LocName) != m_Locations[m_CurrentSub].end()) {
                ERROR("already defined location ", LocName.c_str());
            }
            m_Locations[m_CurrentSub][LocName] = m_WrittenBytes;
            break;
        }

        case ETokenKind::KwReturn: {
            OEmit(OP::RET);
            break;
        }

         case ETokenKind::KwGoto: {
            EXPECT(ETokenKind::Identifier);
            auto LocName = PeekToken();
            OEmitL((OP) OP_EX::GOTO, 0, (m_CurrentSub + ":" + LocName.m_Raw));
            break;
        }
        
        case ETokenKind::KwEmpty: {
            OEmitLR(OP::MOV, STACK_PTR, STACK_BASE);
            OEmitLR(MEMR(OP::MOV), STACK_PTR, 0); // for safety purposes
            break;
        }

        case ETokenKind::KwDrop: {
            PopStack();
            break;
        }

        case ETokenKind::KwDuplicate: {
            OEmitLR(MEMR(OP::LOAD), REG::BX, STACK_PTR);
            PushStack(0);
            OEmitLR(MEMLR(OP::MOV), STACK_PTR, REG::BX);
            break;
        }

        case ETokenKind::KwSwap: {
            OEmitLR(MEMR(OP::LOAD), REG::BX, STACK_PTR);
            OEmitLR(OP::SUB, STACK_PTR, 1);
            OEmitLR(MEMR(OP::LOAD), REG::CX, STACK_PTR);
            OEmitLR(MEMLR(OP::MOV), STACK_PTR, REG::BX);
            OEmitLR(OP::ADD, STACK_PTR, 1);
            OEmitLR(MEMLR(OP::MOV), STACK_PTR, REG::CX);
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
            DoOperator(OP::ADD);
            break;
        }

        case ETokenKind::OpSub: {
            DoOperator(OP::SUB);
            break;
        }

        case ETokenKind::OpMul: {
            DoOperator(OP::MUL);
            break;
        }

        case ETokenKind::OpDiv: {
            DoOperator(OP::DIV);
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
            break;
        }

        case ETokenKind::Illegal: {
            ERROR("illegal statement ", Token.m_Raw.c_str());
            break;
        }
    }
}
