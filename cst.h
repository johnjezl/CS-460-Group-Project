#pragma once

#include <iosfwd>
#include <string>

// Concrete Syntax Tree node stored as an LCRS binary tree.
// leftChild    = first child
// rightSibling = next sibling
struct CSTNode {
    std::string label;
    int line;
    CSTNode* leftChild;
    CSTNode* rightSibling;

    CSTNode(const std::string& text, int lineNumber = 0)
        : label(text),
        line(lineNumber),
        leftChild(nullptr),
        rightSibling(nullptr) {
    }
};

CSTNode* make_node(const std::string& label, int line = 0);
void add_child(CSTNode* parent, CSTNode* child);
void add_sibling(CSTNode* node, CSTNode* sibling);

// Final visible output printer: prints the CST in source-like surface order
// to match the expected assignment output files.
void print_cst_surface(const CSTNode* root, std::ostream& out);

// Keep this only if you still want it for debugging.
// You may remove it entirely if you want.
void print_cst_breadth_first(const CSTNode* root, std::ostream& out);

void delete_cst(CSTNode* root);