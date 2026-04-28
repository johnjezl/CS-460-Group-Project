#include "ast.h"
#include <algorithm>
#include <ostream>
#include <string>
#include <vector>

namespace {

constexpr const char* SEP = "   ";

ASTNode* tail_of(ASTNode* node) {
    if (node == nullptr) {
        return nullptr;
    }
    while (node->rightSibling != nullptr) {
        node = node->rightSibling;
    }
    return node;
}

void append_chain(ASTNode*& head, ASTNode* chain) {
    if (chain == nullptr) {
        return;
    }
    if (head == nullptr) {
        head = chain;
        return;
    }
    tail_of(head)->rightSibling = chain;
}

std::vector<const CSTNode*> children_of(const CSTNode* node) {
    std::vector<const CSTNode*> children;
    for (const CSTNode* child = node ? node->leftChild : nullptr;
         child != nullptr;
         child = child->rightSibling) {
        children.push_back(child);
    }
    return children;
}

bool is_leaf(const CSTNode* node) {
    return node != nullptr && node->leftChild == nullptr;
}

bool is_token(const CSTNode* node, const std::string& text) {
    return is_leaf(node) && node->label == text;
}

std::string join_tokens(const std::vector<std::string>& tokens) {
    std::string out;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i != 0) {
            out += SEP;
        }
        out += tokens[i];
    }
    return out;
}

std::string trim_copy(const std::string& s) {
    const auto first = std::find_if_not(s.begin(), s.end(), [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    });
    if (first == s.end()) {
        return "";
    }
    const auto last = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }).base();
    return std::string(first, last);
}

bool is_binary_expr_label(const std::string& label) {
    return label == "assignment_expr"
        || label == "logical_or_expr"
        || label == "logical_and_expr"
        || label == "equality_expr"
        || label == "relational_expr"
        || label == "additive_expr"
        || label == "multiplicative_expr";
}

void append_tokens(std::vector<std::string>& dst, const std::vector<std::string>& src) {
    dst.insert(dst.end(), src.begin(), src.end());
}

std::vector<std::string> expr_tokens(const CSTNode* node);

std::vector<std::string> literal_tokens(const CSTNode* node,
                                        const std::string& quoteToken) {
    std::vector<std::string> out;
    const auto children = children_of(node);
    if (!children.empty()) {
        out.push_back(quoteToken);
        out.push_back(children[0]->label);
        out.push_back(quoteToken);
    }
    return out;
}

std::vector<std::string> identifier_expr_tokens(const CSTNode* node) {
    std::vector<std::string> out;
    const auto children = children_of(node);
    if (children.empty()) {
        return out;
    }

    out.push_back(children[0]->label);

    for (std::size_t i = 1; i < children.size(); ++i) {
        const CSTNode* child = children[i];

        if (is_token(child, "(")) {
            out.push_back("(");
            ++i;
            while (i < children.size() && !is_token(children[i], ")")) {
                if (!is_token(children[i], ",")) {
                    append_tokens(out, expr_tokens(children[i]));
                }
                ++i;
            }
            out.push_back(")");
        }
        else if (is_token(child, "[")) {
            out.push_back("[");
            ++i;
            while (i < children.size() && !is_token(children[i], "]")) {
                append_tokens(out, expr_tokens(children[i]));
                ++i;
            }
            out.push_back("]");
        }
    }

    return out;
}

std::vector<std::string> expr_tokens(const CSTNode* node) {
    if (node == nullptr) {
        return {};
    }

    if (is_leaf(node)) {
        return { node->label };
    }

    const auto children = children_of(node);

    if (is_binary_expr_label(node->label) && children.size() == 3) {
        std::vector<std::string> out = expr_tokens(children[0]);
        append_tokens(out, expr_tokens(children[2]));
        out.push_back(children[1]->label);
        return out;
    }

    if (node->label == "unary_expr") {
        if (children.size() == 2) {
            const std::string op = children[0]->label;

            // The supplied expected outputs treat signed integer constants as
            // one literal token, e.g. -1, but treat boolean NOT as postfix.
            if ((op == "-" || op == "+")
                && children[1]->label == "integer_literal") {
                const auto literal = children_of(children[1]);
                if (!literal.empty()) {
                    return { op + literal[0]->label };
                }
            }

            std::vector<std::string> out = expr_tokens(children[1]);
            out.push_back(op);
            return out;
        }
    }

    if (node->label == "grouped_expr") {
        if (children.size() >= 2) {
            return expr_tokens(children[1]);
        }
        return {};
    }

    if (node->label == "identifier_expr") {
        return identifier_expr_tokens(node);
    }

    if (node->label == "integer_literal" || node->label == "boolean_literal") {
        if (!children.empty()) {
            return { children[0]->label };
        }
        return {};
    }

    if (node->label == "string_literal") {
        return literal_tokens(node, "\"");
    }

    if (node->label == "char_literal") {
        return literal_tokens(node, "'");
    }

    // Fallback: flatten children. This keeps the printer resilient if the CST
    // contains a harmless wrapper node.
    std::vector<std::string> out;
    for (const CSTNode* child : children) {
        append_tokens(out, expr_tokens(child));
    }
    return out;
}

std::vector<std::string> lhs_tokens_from_assignment_stmt(const std::vector<const CSTNode*>& children,
                                                         std::size_t& index) {
    std::vector<std::string> lhs;
    if (index >= children.size()) {
        return lhs;
    }

    lhs.push_back(children[index]->label);
    ++index;

    if (index < children.size() && is_token(children[index], "[")) {
        lhs.push_back("[");
        ++index;
        while (index < children.size() && !is_token(children[index], "]")) {
            append_tokens(lhs, expr_tokens(children[index]));
            ++index;
        }
        if (index < children.size() && is_token(children[index], "]")) {
            lhs.push_back("]");
            ++index;
        }
    }

    return lhs;
}

ASTNode* ast_declaration_chain(const CSTNode* node) {
    const auto children = children_of(node);
    ASTNode* head = nullptr;

    // child 0 is the type_specifier. Every identifier that starts a declarator
    // becomes one abstract DECLARATION node; array size literals are skipped.
    for (std::size_t i = 1; i < children.size(); ++i) {
        if (is_token(children[i], ";") || is_token(children[i], ",")) {
            continue;
        }
        if (is_token(children[i], "[")) {
            while (i < children.size() && !is_token(children[i], "]")) {
                ++i;
            }
            continue;
        }
        if (is_leaf(children[i])) {
            append_chain(head, make_ast_node("DECLARATION", children[i]->line));
        }
    }

    return head;
}

ASTNode* build_statement(const CSTNode* node);

ASTNode* build_block(const CSTNode* node) {
    ASTNode* block = make_ast_node("BEGIN BLOCK", node ? node->line : 0);
    ASTNode* body = nullptr;

    const auto children = children_of(node);
    for (const CSTNode* child : children) {
        if (is_token(child, "{") || is_token(child, "}")) {
            continue;
        }
        append_chain(body, build_statement(child));
    }

    append_chain(body, make_ast_node("END BLOCK", node ? node->line : 0));
    add_ast_child(block, body);
    return block;
}

ASTNode* build_assignment_or_call_stmt(const CSTNode* node) {
    const auto children = children_of(node);
    if (children.empty()) {
        return nullptr;
    }

    const std::string first = children[0]->label;
    std::size_t i = 0;

    if (first == "printf") {
        std::vector<std::string> tokens;
        tokens.push_back("PRINTF");

        // Skip printf and opening paren.
        i = 1;
        if (i < children.size() && is_token(children[i], "(")) {
            ++i;
        }

        while (i < children.size() && !is_token(children[i], ")")) {
            if (is_token(children[i], ",")) {
                ++i;
                continue;
            }

            if (children[i]->label == "string_literal") {
                const auto literal = children_of(children[i]);
                if (!literal.empty()) {
                    tokens.push_back(trim_copy(literal[0]->label));
                }
            }
            else {
                append_tokens(tokens, expr_tokens(children[i]));
            }
            ++i;
        }

        return make_ast_node(join_tokens(tokens), children[0]->line);
    }

    // Assignment statement.
    i = 0;
    std::vector<std::string> lhs = lhs_tokens_from_assignment_stmt(children, i);
    if (i < children.size() && is_token(children[i], "=")) {
        ++i;
        std::vector<std::string> tokens;
        tokens.push_back("ASSIGNMENT");
        append_tokens(tokens, lhs);
        if (i < children.size()) {
            append_tokens(tokens, expr_tokens(children[i]));
        }
        tokens.push_back("=");
        return make_ast_node(join_tokens(tokens), children[0]->line);
    }

    // Procedure/function call statement.
    std::vector<std::string> tokens;
    tokens.push_back("CALL");

    tokens.push_back(first);


    i = 1;
    if (i < children.size() && is_token(children[i], "(")) {
        tokens.push_back("(");
        ++i;
    }

    while (i < children.size() && !is_token(children[i], ")")) {
        if (!is_token(children[i], ",")) {
            append_tokens(tokens, expr_tokens(children[i]));
        }
        ++i;
    }

    if (i < children.size() && is_token(children[i], ")")) {
        tokens.push_back(")");
    }

    return make_ast_node(join_tokens(tokens), children[0]->line);
}

ASTNode* build_if_stmt(const CSTNode* node) {
    const auto children = children_of(node);
    std::vector<std::string> tokens;
    tokens.push_back("IF");

    std::size_t i = 0;
    if (i < children.size() && is_token(children[i], "if")) {
        ++i;
    }
    if (i < children.size() && is_token(children[i], "(")) {
        ++i;
    }
    if (i < children.size()) {
        append_tokens(tokens, expr_tokens(children[i]));
        ++i;
    }
    if (i < children.size() && is_token(children[i], ")")) {
        ++i;
    }

    ASTNode* ifNode = make_ast_node(join_tokens(tokens), node ? node->line : 0);

    if (i < children.size()) {
        add_ast_child(ifNode, build_statement(children[i]));
        ++i;
    }

    if (i < children.size() && is_token(children[i], "else")) {
        ASTNode* elseNode = make_ast_node("ELSE", children[i]->line);
        if (i + 1 < children.size()) {
            add_ast_child(elseNode, build_statement(children[i + 1]));
        }
        add_ast_sibling(ifNode->leftChild, elseNode);
    }

    return ifNode;
}

ASTNode* build_while_stmt(const CSTNode* node) {
    const auto children = children_of(node);
    std::vector<std::string> tokens;
    tokens.push_back("WHILE");

    std::size_t i = 0;
    if (i < children.size() && is_token(children[i], "while")) {
        ++i;
    }
    if (i < children.size() && is_token(children[i], "(")) {
        ++i;
    }
    if (i < children.size()) {
        append_tokens(tokens, expr_tokens(children[i]));
        ++i;
    }

    ASTNode* whileNode = make_ast_node(join_tokens(tokens), node ? node->line : 0);

    while (i < children.size() && !is_token(children[i], ")")) {
        ++i;
    }
    if (i < children.size() && is_token(children[i], ")")) {
        ++i;
    }
    if (i < children.size()) {
        add_ast_child(whileNode, build_statement(children[i]));
    }

    return whileNode;
}

ASTNode* build_for_stmt(const CSTNode* node) {
    const auto children = children_of(node);
    ASTNode* head = nullptr;

    std::size_t i = 0;
    if (i < children.size() && is_token(children[i], "for")) {
        ++i;
    }
    if (i < children.size() && is_token(children[i], "(")) {
        ++i;
    }

    for (int exprNo = 1; exprNo <= 3; ++exprNo) {
        std::vector<std::string> tokens;
        tokens.push_back("FOR EXPRESSION " + std::to_string(exprNo));

        if (i < children.size()
            && !is_token(children[i], ";")
            && !is_token(children[i], ")")) {
            append_tokens(tokens, expr_tokens(children[i]));
            ++i;
        }

        append_chain(head, make_ast_node(join_tokens(tokens), node ? node->line : 0));

        if (exprNo < 3 && i < children.size() && is_token(children[i], ";")) {
            ++i;
        }
    }

    if (i < children.size() && is_token(children[i], ")")) {
        ++i;
    }
    if (i < children.size()) {
        append_chain(head, build_statement(children[i]));
    }

    return head;
}

ASTNode* build_return_stmt(const CSTNode* node) {
    const auto children = children_of(node);
    std::vector<std::string> tokens;
    tokens.push_back("RETURN");

    for (std::size_t i = 1; i < children.size(); ++i) {
        if (!is_token(children[i], ";")) {
            append_tokens(tokens, expr_tokens(children[i]));
        }
    }

    return make_ast_node(join_tokens(tokens), node ? node->line : 0);
}

ASTNode* build_subprogram_decl(const CSTNode* node) {
    ASTNode* decl = make_ast_node("DECLARATION", node ? node->line : 0);
    for (const CSTNode* child : children_of(node)) {
        if (child->label == "block") {
            add_ast_child(decl, build_block(child));
            break;
        }
    }
    return decl;
}

ASTNode* build_statement(const CSTNode* node) {
    if (node == nullptr) {
        return nullptr;
    }

    if (node->label == "declaration_stmt") {
        return ast_declaration_chain(node);
    }
    if (node->label == "assignment_or_call_stmt") {
        return build_assignment_or_call_stmt(node);
    }
    if (node->label == "if_stmt") {
        return build_if_stmt(node);
    }
    if (node->label == "while_stmt") {
        return build_while_stmt(node);
    }
    if (node->label == "for_stmt") {
        return build_for_stmt(node);
    }
    if (node->label == "return_stmt") {
        return build_return_stmt(node);
    }
    if (node->label == "block") {
        return build_block(node);
    }
    if (node->label == "function_decl" || node->label == "procedure_decl") {
        return build_subprogram_decl(node);
    }

    return nullptr;
}

void print_ast_preorder(const ASTNode* node, std::ostream& out) {
    if (node == nullptr) {
        return;
    }
    out << node->label << '\n';
    print_ast_preorder(node->leftChild, out);
    print_ast_preorder(node->rightSibling, out);
}

} // namespace

ASTNode* make_ast_node(const std::string& label, int line) {
    return new ASTNode(label, line);
}

void add_ast_child(ASTNode* parent, ASTNode* child) {
    if (parent == nullptr || child == nullptr) {
        return;
    }
    if (parent->leftChild == nullptr) {
        parent->leftChild = child;
        return;
    }
    tail_of(parent->leftChild)->rightSibling = child;
}

void add_ast_sibling(ASTNode* node, ASTNode* sibling) {
    if (node == nullptr || sibling == nullptr) {
        return;
    }
    tail_of(node)->rightSibling = sibling;
}

ASTNode* build_ast_from_cst(const CSTNode* cstRoot) {
    ASTNode* astRoot = make_ast_node("program", cstRoot ? cstRoot->line : 0);
    ASTNode* topLevel = nullptr;

    for (const CSTNode* child : children_of(cstRoot)) {
        append_chain(topLevel, build_statement(child));
    }

    add_ast_child(astRoot, topLevel);
    return astRoot;
}

void print_ast(const ASTNode* root, std::ostream& out) {
    if (root == nullptr) {
        return;
    }

    // The provided Assignment 5 expected files list the AST in source/semantic
    // preorder, starting with the program node's children, and omit the artificial
    // root label.
    print_ast_preorder(root->leftChild, out);
}

void delete_ast(ASTNode* root) {
    if (root == nullptr) {
        return;
    }
    delete_ast(root->leftChild);
    delete_ast(root->rightSibling);
    delete root;
}
