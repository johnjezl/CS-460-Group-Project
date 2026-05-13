#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "comment_stripper.h"
#include "lexer.h"
#include "parser.h"
#include "cst.h"
#include "interpreter.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: assignment6 <input_file>\n";
        return 1;
    }

    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cout << "ERROR: Cannot open input file\n";
        return 1;
    }

    std::ostringstream cleaned_out;
    StripResult sr = strip_comments(in, cleaned_out);
    if (!sr.ok) {
        std::cout << sr.error_message << "\n";
        return 1;
    }

    std::string cleaned = cleaned_out.str();

    std::string err;
    std::vector<Token> toks = tokenize(cleaned, err);
    if (!err.empty()) {
        std::cout << err << "\n";
        return 1;
    }

    CSTNode* root = nullptr;
    try {
        Parser parser(toks);
        root = parser.parse_program();
    }
    catch (const SemanticError& e) {
        std::cout << e.what() << "\n";
        return 1;
    }
    catch (const ParseError& e) {
        std::cout << e.what() << "\n";
        return 1;
    }

    InterpretResult ir = interpret(root, std::cout);
    if (!ir.ok) {
        std::cerr << ir.error_message << "\n";
        delete_cst(root);
        return 1;
    }

    delete_cst(root);
    return 0;
}
