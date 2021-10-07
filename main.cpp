#include "lexer.hpp"
#include "parser.hpp"

#include <iostream>
#include <fstream>

void PrintUsage() {
    std::cout << "Usage:\n\t";
    std::cout <<    "...      <input>\n\t";
    std::cout <<    "--output <file>";
    std::cout << "\n";
}

void PrintVersion() {
    std::cout << "\033[95mPriml\033[0m compiler 1.0\n";
    std::cout << "Copyright (C) 2021 Addy, Republic of Kosovo\n";
    std::cout << "Available at \033[4;94mhttps://www.github.com/x-Eagle-x/Priml/\033[0m\n\n";
}

int main(int argc, char *args[]) {
    if (argc <= 1) {
        PrintUsage();
        return 0;
    }

    CLexer Lexer;
    CParser Parser;

    std::string Input{}, Output = "./out.b";
    std::ifstream InputStream;

    for (int arg = 1; arg < argc; arg++) {
        std::string ArgType(args[arg]);

        if (ArgType == "--output") {
            Output = std::string(args[++arg]);
            continue;
        }
        else if (ArgType == "--std") {
            args[arg] = (char*) "./std.prl";
        }
        else if (ArgType == "--version") {
            PrintVersion();
            return 0;
        }
        else if (ArgType == "--help") {
            PrintUsage();
            return 0;
        }

        InputStream.open(args[arg], std::ios::in);
        if (!InputStream.is_open()) {
            std::cout << "\033[91mError\033[0m: couldn't open input file " << args[arg] << ".\n";
            return 1;
        }

        for (std::string Line; std::getline(InputStream, Line); ) {
            Input += (Line + "\n");
        }

        Input += EOF;
        Parser.AddFile(std::string(args[arg]));
        InputStream.close();
    }

    Lexer.Feed(Input);
    Lexer.Lex(); 

    Parser.Feed(Lexer);
    Parser.InitFile(Output);
    Parser.StartParsing();
    Parser.Close();
}