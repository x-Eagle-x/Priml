#ifndef _PARSER_
#define _PARSER_

#include <fstream>
#include <unordered_map>

#include "lexer.hpp"
#include "opcodes.hpp"

union uint32_byte
{
    uint8_t Byte[4];
    uint32_t Integer;
    
    uint8_t operator[](uint8_t Byte) {
        return this->Byte[Byte];
    }

    uint32_byte(uint32_t Integer) {
        this->Integer = Integer;
    }
};

#define STACK_BASE_PTR 4
#define PROGRAM_SIZE 4069

struct SInstruction {
    OP m_Opcode;
    uint8_t m_Operands;
    uint32_t m_Left, m_Right;
};

class CParser {
    uint32_t m_Line = 0, m_WrittenBytes = 5;

    std::size_t m_Index = 0, m_CurrentFile = 0;
    std::string m_OutFile;

    std::ofstream m_OutStream;
    std::vector<std::string> m_InputFiles;
    std::vector<SInstruction> m_OutOverhead;

    CLexer *m_Lexer;

    bool m_FailedCompilation = false;
    uint32_t m_GlobalCallId = 0, m_StackStart = 0x0F, m_StackPtr = 0x0F;

    std::map<std::string, uint32_t> m_ExternSymbols;
    std::map<std::string, uint32_t> m_SubSymbols;
    std::map<std::string, std::vector<CToken>> m_Macros;
    std::map<std::string, std::map<std::string, uint32_t>> m_Locations;

    std::string m_CurrentSub;

public:
    CToken PeekToken(std::size_t Peek = 1);
    CToken TempPeekToken(std::size_t Peek = 1);
    CToken CurrentToken();

    void OEmit(OP Opcode);
    void OEmitL(OP Opcode, uint32_t Left);
    void OEmitLR(OP Opcode, uint32_t Left, uint32_t Right);

    void WriteBytes(uint32_t Source);
    void Emit(OP Opcode);
    void EmitL(OP Opcode, uint32_t Left);
    void EmitLR(OP Opcode, uint32_t Left, uint32_t Right);

    void Fail();
    void Feed(CLexer &Lexer);
    void AddFile(std::string Name);
    void InitFile(std::string Output);
    void Close();
    void StartParsing();
    void Parse();
};

#endif