#pragma once

#include <iosfwd>
#include <string>

#include "cst.h"

// Abstract Syntax Tree node stored as an LCRS binary tree.
// leftChild    = first child
// rightSibling = next sibling
struct ASTNode {
    std::string label;
    int line;
    ASTNode* leftChild;
    ASTNode* rightSibling;

    ASTNode(const std::string& text, int lineNumber = 0)
        : label(text),
          line(lineNumber),
          leftChild(nullptr),
          rightSibling(nullptr) {
    }
};

ASTNode* make_ast_node(const std::string& label, int line = 0);
void add_ast_child(ASTNode* parent, ASTNode* child);
void add_ast_sibling(ASTNode* node, ASTNode* sibling);

ASTNode* build_ast_from_cst(const CSTNode* cstRoot);
void print_ast(const ASTNode* root, std::ostream& out);
void delete_ast(ASTNode* root);
