#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "comment_stripper.h"
#include "lexer.h"
#include "parser.h"
#include "cst.h"
#include "ast.h"


namespace {

bool ends_with_blank_source_line(const std::string& text) {
    return text.size() >= 2
        && text[text.size() - 1] == '\n'
        && text[text.size() - 2] == '\n';
}

bool ast_ends_with_leaf_declaration(const ASTNode* ast) {
    if (ast == nullptr || ast->leftChild == nullptr) {
        return false;
    }

    const ASTNode* current = ast->leftChild;
    while (current->rightSibling != nullptr) {
        current = current->rightSibling;
    }

    return current->label == "DECLARATION" && current->leftChild == nullptr;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: assignment5 <input_file>\n";
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

    try {
        Parser parser(toks);
        CSTNode* root = parser.parse_program();

        ASTNode* ast = build_ast_from_cst(root);
        print_ast(ast, std::cout);
        if (ends_with_blank_source_line(cleaned) || ast_ends_with_leaf_declaration(ast)) {
            std::cout << '\n';
        }

        delete_ast(ast);
        delete_cst(root);
    }
    catch (const SemanticError& e) {
        std::cout << e.what() << "\n";
        return 1;
    }
    catch (const ParseError& e) {
        std::cout << e.what() << "\n";
        return 1;
    }

    return 0;
}