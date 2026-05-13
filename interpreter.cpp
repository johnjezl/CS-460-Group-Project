#include "interpreter.h"

#include <cstddef>
#include <cstdlib>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

// ---------- Runtime value ----------
struct Value {
    enum class Tag { INT, STRING, ARRAY };
    Tag tag = Tag::INT;
    long long i = 0;
    std::string s;
    std::vector<long long> arr;
};

// ---------- Subprogram metadata ----------
struct Param {
    std::string name;
    std::string type;
    bool isArray = false;
};

struct SubprogramDef {
    std::string name;
    bool isFunction = false;
    std::vector<Param> params;
    const CSTNode* block = nullptr;
};

// ---------- Scope frame ----------
struct Scope {
    std::unordered_map<std::string, Value> vars;
};

// ---------- Postfix token kinds ----------
enum class TokKind {
    INT_LIT,
    STRING_LIT,
    IDENT,
    INDEX,
    CALL,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_AND, OP_OR, OP_NOT,
    OP_NEG, OP_POS
};

struct PostfixTok {
    TokKind kind;
    long long i = 0;
    std::string s;
    int argc = 0;
};

class ReturnSignal {
public:
    Value value;
};

// ---------- CST helpers ----------
std::vector<const CSTNode*> children_of(const CSTNode* node) {
    std::vector<const CSTNode*> out;
    for (const CSTNode* c = node ? node->leftChild : nullptr;
         c != nullptr;
         c = c->rightSibling) {
        out.push_back(c);
    }
    return out;
}

bool is_leaf_token(const CSTNode* n, const std::string& text) {
    return n != nullptr && n->leftChild == nullptr && n->label == text;
}

// ---------- Escape decoding ----------
int hex_digit_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

std::string decode_escapes(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    for (std::size_t i = 0; i < src.size(); ) {
        char c = src[i];
        if (c == '\\' && i + 1 < src.size()) {
            char e = src[i + 1];
            if (e == 'x') {
                int val = 0;
                std::size_t j = i + 2;
                int n = 0;
                while (j < src.size() && n < 2 && hex_digit_value(src[j]) >= 0) {
                    val = val * 16 + hex_digit_value(src[j]);
                    ++j;
                    ++n;
                }
                out.push_back(static_cast<char>(val));
                i = j;
                continue;
            }
            switch (e) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case 'a': out.push_back('\a'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'v': out.push_back('\v'); break;
            case '\\': out.push_back('\\'); break;
            case '\'': out.push_back('\''); break;
            case '"': out.push_back('"'); break;
            case '?': out.push_back('?'); break;
            case '0': out.push_back('\0'); break;
            default: out.push_back(e); break;
            }
            i += 2;
        }
        else {
            out.push_back(c);
            ++i;
        }
    }
    return out;
}

int decode_char_literal(const std::string& src) {
    if (src.empty()) return 0;
    if (src[0] != '\\') {
        return static_cast<unsigned char>(src[0]);
    }
    if (src.size() < 2) return static_cast<unsigned char>('\\');
    char e = src[1];
    if (e == 'x') {
        int val = 0;
        for (std::size_t j = 2; j < src.size(); ++j) {
            int h = hex_digit_value(src[j]);
            if (h < 0) break;
            val = val * 16 + h;
        }
        return val;
    }
    switch (e) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case 'a': return '\a';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'v': return '\v';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"': return '"';
    case '?': return '?';
    case '0': return 0;
    default: return static_cast<unsigned char>(e);
    }
}

// ---------- Interpreter ----------
class Interpreter {
public:
    Interpreter(const CSTNode* program, std::ostream& out)
        : program_(program), out_(out) {}

    void run();

private:
    const CSTNode* program_;
    std::ostream& out_;
    std::unordered_map<std::string, SubprogramDef> subprograms_;
    std::unordered_map<std::string, Value> globals_;
    std::vector<Scope> scopes_;

    void collect_subprograms_();
    void exec_global_decls_();

    Value call_(const std::string& name, std::vector<Value> args, int line);
    void exec_block_(const CSTNode* block);
    void exec_stmt_(const CSTNode* stmt);
    void exec_decl_stmt_(const CSTNode* stmt, bool global);
    void exec_assign_or_call_stmt_(const CSTNode* stmt);
    void exec_if_stmt_(const CSTNode* stmt);
    void exec_while_stmt_(const CSTNode* stmt);
    void exec_for_stmt_(const CSTNode* stmt);
    void exec_return_stmt_(const CSTNode* stmt);
    void do_printf_(const std::vector<const CSTNode*>& children);

    Value eval_(const CSTNode* expr);
    Value eval_assignment_expr_(const CSTNode* expr);
    Value eval_postfix_(const std::vector<PostfixTok>& tokens, int line);
    void emit_postfix_(const CSTNode* node, std::vector<PostfixTok>& out);

    Value& lookup_var_ref_(const std::string& name, int line);
    Value lookup_var_(const std::string& name, int line);

    static int parse_array_size_node_(const CSTNode* sizeNode);
};

int Interpreter::parse_array_size_node_(const CSTNode* sizeNode) {
    const CSTNode* lit = sizeNode;
    if (lit->label == "unary_expr") {
        auto ch = children_of(lit);
        if (ch.size() >= 2) {
            lit = ch[1];
        }
    }
    auto litCh = children_of(lit);
    if (litCh.empty()) return 0;
    try {
        return std::stoi(litCh[0]->label);
    }
    catch (...) {
        return 0;
    }
}

Value& Interpreter::lookup_var_ref_(const std::string& name, int line) {
    if (!scopes_.empty()) {
        auto& vars = scopes_.back().vars;
        auto it = vars.find(name);
        if (it != vars.end()) return it->second;
    }
    auto it = globals_.find(name);
    if (it != globals_.end()) return it->second;
    throw std::runtime_error(
        "undefined variable \"" + name + "\" on line " + std::to_string(line));
}

Value Interpreter::lookup_var_(const std::string& name, int line) {
    return lookup_var_ref_(name, line);
}

void Interpreter::collect_subprograms_() {
    for (const CSTNode* d : children_of(program_)) {
        if (d->label == "function_decl") {
            // children: [function, type_specifier, name, (, parameter_list, ), block]
            auto ch = children_of(d);
            if (ch.size() < 7) continue;
            SubprogramDef sp;
            sp.name = ch[2]->label;
            sp.isFunction = true;
            sp.block = ch[6];

            for (const CSTNode* p : children_of(ch[4])) {
                if (p->label != "parameter") continue;
                auto pc = children_of(p);
                if (pc.size() < 2) continue;
                Param par;
                auto typeCh = children_of(pc[0]);
                par.type = typeCh.empty() ? "" : typeCh[0]->label;
                par.name = pc[1]->label;
                par.isArray = (pc.size() >= 3 && pc[2]->label == "[");
                sp.params.push_back(par);
            }

            subprograms_[sp.name] = sp;
        }
        else if (d->label == "procedure_decl") {
            // children: [procedure, name, (, parameter_list, ), block]
            auto ch = children_of(d);
            if (ch.size() < 6) continue;
            SubprogramDef sp;
            sp.name = ch[1]->label;
            sp.isFunction = false;
            sp.block = ch[5];

            for (const CSTNode* p : children_of(ch[3])) {
                if (p->label != "parameter") continue;
                auto pc = children_of(p);
                if (pc.size() < 2) continue;
                Param par;
                auto typeCh = children_of(pc[0]);
                par.type = typeCh.empty() ? "" : typeCh[0]->label;
                par.name = pc[1]->label;
                par.isArray = (pc.size() >= 3 && pc[2]->label == "[");
                sp.params.push_back(par);
            }

            subprograms_[sp.name] = sp;
        }
    }
}

void Interpreter::exec_global_decls_() {
    for (const CSTNode* d : children_of(program_)) {
        if (d->label == "declaration_stmt") {
            exec_decl_stmt_(d, true);
        }
    }
}

void Interpreter::run() {
    collect_subprograms_();
    exec_global_decls_();

    auto it = subprograms_.find("main");
    if (it == subprograms_.end()) {
        return;
    }
    call_("main", {}, 0);
}

Value Interpreter::call_(const std::string& name, std::vector<Value> args, int line) {
    auto it = subprograms_.find(name);
    if (it == subprograms_.end()) {
        throw std::runtime_error(
            "undefined subprogram \"" + name + "\" on line " + std::to_string(line));
    }
    const SubprogramDef& sp = it->second;

    Scope frame;
    for (std::size_t i = 0; i < sp.params.size() && i < args.size(); ++i) {
        const Param& p = sp.params[i];
        Value v;
        if (p.isArray || args[i].tag == Value::Tag::ARRAY) {
            v.tag = Value::Tag::ARRAY;
            v.arr = args[i].arr;
        }
        else {
            v.tag = Value::Tag::INT;
            v.i = args[i].i;
        }
        frame.vars[p.name] = std::move(v);
    }

    scopes_.push_back(std::move(frame));
    Value result;
    result.tag = Value::Tag::INT;
    result.i = 0;
    try {
        exec_block_(sp.block);
    }
    catch (const ReturnSignal& rs) {
        result = rs.value;
    }
    scopes_.pop_back();
    return result;
}

void Interpreter::exec_block_(const CSTNode* block) {
    if (block == nullptr) return;
    for (const CSTNode* c : children_of(block)) {
        if (is_leaf_token(c, "{") || is_leaf_token(c, "}")) continue;
        exec_stmt_(c);
    }
}

void Interpreter::exec_stmt_(const CSTNode* stmt) {
    if (stmt == nullptr) return;
    const std::string& l = stmt->label;
    if (l == "declaration_stmt")        exec_decl_stmt_(stmt, false);
    else if (l == "assignment_or_call_stmt") exec_assign_or_call_stmt_(stmt);
    else if (l == "if_stmt")             exec_if_stmt_(stmt);
    else if (l == "while_stmt")          exec_while_stmt_(stmt);
    else if (l == "for_stmt")            exec_for_stmt_(stmt);
    else if (l == "return_stmt")         exec_return_stmt_(stmt);
    else if (l == "block")               exec_block_(stmt);
}

void Interpreter::exec_decl_stmt_(const CSTNode* stmt, bool global) {
    auto ch = children_of(stmt);
    if (ch.empty()) return;

    std::size_t i = 1;
    while (i < ch.size()) {
        const std::string& lbl = ch[i]->label;
        if (lbl == "," || lbl == ";") {
            ++i;
            continue;
        }

        std::string name = lbl;
        ++i;

        bool isArray = false;
        int arraySize = 0;
        if (i < ch.size() && ch[i]->label == "[") {
            isArray = true;
            ++i; // skip [
            if (i < ch.size()) {
                arraySize = parse_array_size_node_(ch[i]);
                ++i; // skip size node
            }
            if (i < ch.size() && ch[i]->label == "]") {
                ++i; // skip ]
            }
        }

        Value v;
        if (isArray) {
            v.tag = Value::Tag::ARRAY;
            v.arr.assign(arraySize, 0);
        }
        else {
            v.tag = Value::Tag::INT;
            v.i = 0;
        }

        if (global) {
            globals_[name] = std::move(v);
        }
        else {
            scopes_.back().vars[name] = std::move(v);
        }
    }
}

void Interpreter::exec_assign_or_call_stmt_(const CSTNode* stmt) {
    auto ch = children_of(stmt);
    if (ch.empty()) return;

    if (ch[0]->label == "printf") {
        do_printf_(ch);
        return;
    }

    const std::string name = ch[0]->label;

    // Indexed assignment: name [ idx_expr ] = expr ;
    if (ch.size() >= 2 && ch[1]->label == "[") {
        // children: [name, [, idx_expr, ], =, expr, ;]
        Value idx = eval_(ch[2]);
        // ch[3] = ']', ch[4] = '=', ch[5] = expr
        if (ch.size() >= 6) {
            Value rhs = eval_(ch[5]);
            Value& slot = lookup_var_ref_(name, stmt->line);
            if (slot.tag == Value::Tag::ARRAY
                && idx.i >= 0
                && static_cast<std::size_t>(idx.i) < slot.arr.size()) {
                slot.arr[static_cast<std::size_t>(idx.i)] = rhs.i;
            }
        }
        return;
    }

    // Simple assignment: name = expr ;
    if (ch.size() >= 2 && ch[1]->label == "=") {
        // children: [name, =, expr, ;]
        Value rhs = eval_(ch[2]);
        Value& slot = lookup_var_ref_(name, stmt->line);
        if (slot.tag == Value::Tag::ARRAY) {
            if (rhs.tag == Value::Tag::STRING) {
                std::string decoded = decode_escapes(rhs.s);
                for (std::size_t k = 0; k < slot.arr.size(); ++k) {
                    slot.arr[k] = (k < decoded.size())
                                      ? static_cast<unsigned char>(decoded[k])
                                      : 0;
                }
            }
            else if (rhs.tag == Value::Tag::ARRAY) {
                for (std::size_t k = 0; k < slot.arr.size(); ++k) {
                    slot.arr[k] = (k < rhs.arr.size()) ? rhs.arr[k] : 0;
                }
            }
        }
        else {
            slot.tag = Value::Tag::INT;
            slot.i = rhs.i;
        }
        return;
    }

    // Call: name ( args ) ;
    if (ch.size() >= 2 && ch[1]->label == "(") {
        std::vector<Value> args;
        for (std::size_t i = 2; i < ch.size() && ch[i]->label != ")"; ++i) {
            if (ch[i]->label == ",") continue;
            args.push_back(eval_(ch[i]));
        }
        call_(name, std::move(args), stmt->line);
        return;
    }
}

void Interpreter::exec_if_stmt_(const CSTNode* stmt) {
    // children: [if, (, cond, ), then-stmt, [else, else-stmt]?]
    auto ch = children_of(stmt);
    if (ch.size() < 5) return;

    Value cond = eval_(ch[2]);
    if (cond.i != 0) {
        exec_stmt_(ch[4]);
    }
    else if (ch.size() >= 7 && ch[5]->label == "else") {
        exec_stmt_(ch[6]);
    }
}

void Interpreter::exec_while_stmt_(const CSTNode* stmt) {
    // children: [while, (, cond, ), body]
    auto ch = children_of(stmt);
    if (ch.size() < 5) return;

    while (true) {
        Value cond = eval_(ch[2]);
        if (cond.i == 0) break;
        exec_stmt_(ch[4]);
    }
}

void Interpreter::exec_for_stmt_(const CSTNode* stmt) {
    // children: [for, (, [init], ;, [cond], ;, [post], ), body]
    auto ch = children_of(stmt);

    // Walk past "for" "(" to find init/cond/post by semicolons.
    std::size_t pos = 0;
    if (pos < ch.size() && ch[pos]->label == "for") ++pos;
    if (pos < ch.size() && ch[pos]->label == "(") ++pos;

    const CSTNode* initE = nullptr;
    const CSTNode* condE = nullptr;
    const CSTNode* postE = nullptr;
    const CSTNode* body = nullptr;

    if (pos < ch.size() && ch[pos]->label != ";") {
        initE = ch[pos];
        ++pos;
    }
    if (pos < ch.size() && ch[pos]->label == ";") ++pos;

    if (pos < ch.size() && ch[pos]->label != ";") {
        condE = ch[pos];
        ++pos;
    }
    if (pos < ch.size() && ch[pos]->label == ";") ++pos;

    if (pos < ch.size() && ch[pos]->label != ")") {
        postE = ch[pos];
        ++pos;
    }
    if (pos < ch.size() && ch[pos]->label == ")") ++pos;

    if (pos < ch.size()) body = ch[pos];

    if (initE != nullptr) eval_(initE);

    while (true) {
        if (condE != nullptr) {
            Value c = eval_(condE);
            if (c.i == 0) break;
        }
        if (body != nullptr) exec_stmt_(body);
        if (postE != nullptr) eval_(postE);
    }
}

void Interpreter::exec_return_stmt_(const CSTNode* stmt) {
    // children: [return, [expr], ;]
    auto ch = children_of(stmt);
    ReturnSignal rs;
    rs.value.tag = Value::Tag::INT;
    rs.value.i = 0;
    if (ch.size() >= 2 && !is_leaf_token(ch[1], ";")) {
        rs.value = eval_(ch[1]);
    }
    throw rs;
}

void Interpreter::do_printf_(const std::vector<const CSTNode*>& children) {
    std::vector<Value> args;
    bool past_paren = false;
    for (const CSTNode* c : children) {
        if (is_leaf_token(c, "printf")) continue;
        if (is_leaf_token(c, "(")) { past_paren = true; continue; }
        if (!past_paren) continue;
        if (is_leaf_token(c, ")")) break;
        if (is_leaf_token(c, ",")) continue;
        if (is_leaf_token(c, ";")) continue;
        args.push_back(eval_(c));
    }
    if (args.empty()) return;

    std::string fmt = decode_escapes(args[0].s);
    std::size_t arg_i = 1;

    for (std::size_t i = 0; i < fmt.size(); ) {
        char c = fmt[i];
        if (c == '%' && i + 1 < fmt.size()) {
            char spec = fmt[i + 1];
            if (spec == '%') {
                out_ << '%';
                i += 2;
                continue;
            }
            if (arg_i < args.size() && (spec == 'd' || spec == 'c' || spec == 's')) {
                const Value& v = args[arg_i++];
                if (spec == 'd') {
                    out_ << v.i;
                }
                else if (spec == 'c') {
                    out_ << static_cast<char>(v.i);
                }
                else if (spec == 's') {
                    if (v.tag == Value::Tag::STRING) {
                        out_ << decode_escapes(v.s);
                    }
                    else if (v.tag == Value::Tag::ARRAY) {
                        for (long long x : v.arr) {
                            if (x == 0) break;
                            out_ << static_cast<char>(x);
                        }
                    }
                    else {
                        out_ << v.i;
                    }
                }
                i += 2;
                continue;
            }
        }
        out_ << c;
        ++i;
    }
}

Value Interpreter::eval_(const CSTNode* expr) {
    if (expr == nullptr) {
        Value v;
        v.tag = Value::Tag::INT;
        v.i = 0;
        return v;
    }
    if (expr->label == "assignment_expr") {
        return eval_assignment_expr_(expr);
    }
    std::vector<PostfixTok> tokens;
    emit_postfix_(expr, tokens);
    return eval_postfix_(tokens, expr->line);
}

Value Interpreter::eval_assignment_expr_(const CSTNode* expr) {
    // children: [lhs(identifier_expr), =, rhs]
    auto ch = children_of(expr);
    if (ch.size() < 3) {
        Value v;
        v.tag = Value::Tag::INT;
        v.i = 0;
        return v;
    }

    Value rhs = eval_(ch[2]);

    auto lc = children_of(ch[0]);
    if (lc.empty()) return rhs;

    std::string name = lc[0]->label;
    bool indexed = (lc.size() >= 4 && lc[1]->label == "[");

    long long idx = 0;
    if (indexed) {
        Value iv = eval_(lc[2]);
        idx = iv.i;
    }

    Value& slot = lookup_var_ref_(name, expr->line);
    if (indexed) {
        if (slot.tag == Value::Tag::ARRAY
            && idx >= 0
            && static_cast<std::size_t>(idx) < slot.arr.size()) {
            slot.arr[static_cast<std::size_t>(idx)] = rhs.i;
        }
    }
    else {
        if (slot.tag == Value::Tag::ARRAY) {
            if (rhs.tag == Value::Tag::STRING) {
                std::string decoded = decode_escapes(rhs.s);
                for (std::size_t k = 0; k < slot.arr.size(); ++k) {
                    slot.arr[k] = (k < decoded.size())
                                      ? static_cast<unsigned char>(decoded[k])
                                      : 0;
                }
            }
            else if (rhs.tag == Value::Tag::ARRAY) {
                for (std::size_t k = 0; k < slot.arr.size(); ++k) {
                    slot.arr[k] = (k < rhs.arr.size()) ? rhs.arr[k] : 0;
                }
            }
        }
        else {
            slot.tag = Value::Tag::INT;
            slot.i = rhs.i;
        }
    }

    return rhs;
}

void Interpreter::emit_postfix_(const CSTNode* node, std::vector<PostfixTok>& out) {
    if (node == nullptr) return;
    const std::string& lbl = node->label;
    auto ch = children_of(node);

    if (lbl == "integer_literal") {
        if (!ch.empty()) {
            PostfixTok t;
            t.kind = TokKind::INT_LIT;
            try {
                t.i = std::stoll(ch[0]->label);
            }
            catch (...) {
                t.i = 0;
            }
            out.push_back(t);
        }
        return;
    }
    if (lbl == "boolean_literal") {
        if (!ch.empty()) {
            PostfixTok t;
            t.kind = TokKind::INT_LIT;
            t.i = (ch[0]->label == "TRUE") ? 1 : 0;
            out.push_back(t);
        }
        return;
    }
    if (lbl == "char_literal") {
        if (!ch.empty()) {
            PostfixTok t;
            t.kind = TokKind::INT_LIT;
            t.i = decode_char_literal(ch[0]->label);
            out.push_back(t);
        }
        return;
    }
    if (lbl == "string_literal") {
        if (!ch.empty()) {
            PostfixTok t;
            t.kind = TokKind::STRING_LIT;
            t.s = ch[0]->label;
            out.push_back(t);
        }
        return;
    }

    if (lbl == "grouped_expr") {
        if (ch.size() >= 2) emit_postfix_(ch[1], out);
        return;
    }

    if (lbl == "unary_expr") {
        if (ch.size() == 2) {
            emit_postfix_(ch[1], out);
            const std::string& op = ch[0]->label;
            PostfixTok t;
            if (op == "-") t.kind = TokKind::OP_NEG;
            else if (op == "+") t.kind = TokKind::OP_POS;
            else if (op == "!") t.kind = TokKind::OP_NOT;
            else t.kind = TokKind::OP_POS;
            out.push_back(t);
        }
        return;
    }

    if (lbl == "additive_expr" || lbl == "multiplicative_expr"
        || lbl == "relational_expr" || lbl == "equality_expr"
        || lbl == "logical_and_expr" || lbl == "logical_or_expr") {
        if (ch.size() == 3) {
            emit_postfix_(ch[0], out);
            emit_postfix_(ch[2], out);

            const std::string& op = ch[1]->label;
            PostfixTok t;
            if (op == "+") t.kind = TokKind::OP_ADD;
            else if (op == "-") t.kind = TokKind::OP_SUB;
            else if (op == "*") t.kind = TokKind::OP_MUL;
            else if (op == "/") t.kind = TokKind::OP_DIV;
            else if (op == "%") t.kind = TokKind::OP_MOD;
            else if (op == "^") t.kind = TokKind::OP_POW;
            else if (op == "==") t.kind = TokKind::OP_EQ;
            else if (op == "!=") t.kind = TokKind::OP_NE;
            else if (op == "<") t.kind = TokKind::OP_LT;
            else if (op == "<=") t.kind = TokKind::OP_LE;
            else if (op == ">") t.kind = TokKind::OP_GT;
            else if (op == ">=") t.kind = TokKind::OP_GE;
            else if (op == "&&") t.kind = TokKind::OP_AND;
            else if (op == "||") t.kind = TokKind::OP_OR;
            else t.kind = TokKind::OP_ADD;
            out.push_back(t);
        }
        return;
    }

    if (lbl == "identifier_expr") {
        if (ch.empty()) return;
        const std::string name = ch[0]->label;

        if (ch.size() == 1) {
            PostfixTok t;
            t.kind = TokKind::IDENT;
            t.s = name;
            out.push_back(t);
            return;
        }

        if (ch[1]->label == "(") {
            int n = 0;
            for (std::size_t i = 2; i < ch.size() && ch[i]->label != ")"; ++i) {
                if (ch[i]->label == ",") continue;
                emit_postfix_(ch[i], out);
                ++n;
            }
            PostfixTok t;
            t.kind = TokKind::CALL;
            t.s = name;
            t.argc = n;
            out.push_back(t);
            return;
        }

        if (ch[1]->label == "[") {
            if (ch.size() >= 3) emit_postfix_(ch[2], out);
            PostfixTok t;
            t.kind = TokKind::INDEX;
            t.s = name;
            out.push_back(t);
            return;
        }

        PostfixTok t;
        t.kind = TokKind::IDENT;
        t.s = name;
        out.push_back(t);
        return;
    }

    // Fallback: flatten children.
    for (const CSTNode* c : ch) {
        emit_postfix_(c, out);
    }
}

Value Interpreter::eval_postfix_(const std::vector<PostfixTok>& tokens, int line) {
    std::vector<Value> stack;
    auto pop_int = [&]() -> long long {
        Value v = stack.back();
        stack.pop_back();
        return v.i;
    };

    for (const PostfixTok& t : tokens) {
        switch (t.kind) {
        case TokKind::INT_LIT: {
            Value v;
            v.tag = Value::Tag::INT;
            v.i = t.i;
            stack.push_back(std::move(v));
            break;
        }
        case TokKind::STRING_LIT: {
            Value v;
            v.tag = Value::Tag::STRING;
            v.s = t.s;
            stack.push_back(std::move(v));
            break;
        }
        case TokKind::IDENT: {
            stack.push_back(lookup_var_(t.s, line));
            break;
        }
        case TokKind::INDEX: {
            long long idx = pop_int();
            Value arr = lookup_var_(t.s, line);
            Value v;
            v.tag = Value::Tag::INT;
            if (arr.tag == Value::Tag::ARRAY
                && idx >= 0
                && static_cast<std::size_t>(idx) < arr.arr.size()) {
                v.i = arr.arr[static_cast<std::size_t>(idx)];
            }
            stack.push_back(std::move(v));
            break;
        }
        case TokKind::CALL: {
            std::vector<Value> args(t.argc);
            for (int j = t.argc - 1; j >= 0; --j) {
                args[j] = std::move(stack.back());
                stack.pop_back();
            }
            stack.push_back(call_(t.s, std::move(args), line));
            break;
        }
        case TokKind::OP_NEG: {
            long long a = pop_int();
            Value r;
            r.tag = Value::Tag::INT;
            r.i = -a;
            stack.push_back(r);
            break;
        }
        case TokKind::OP_POS: {
            // no-op, value already on stack
            break;
        }
        case TokKind::OP_NOT: {
            long long a = pop_int();
            Value r;
            r.tag = Value::Tag::INT;
            r.i = (a == 0) ? 1 : 0;
            stack.push_back(r);
            break;
        }
        case TokKind::OP_ADD: case TokKind::OP_SUB: case TokKind::OP_MUL:
        case TokKind::OP_DIV: case TokKind::OP_MOD: case TokKind::OP_POW:
        case TokKind::OP_EQ:  case TokKind::OP_NE:
        case TokKind::OP_LT:  case TokKind::OP_LE:
        case TokKind::OP_GT:  case TokKind::OP_GE:
        case TokKind::OP_AND: case TokKind::OP_OR: {
            long long b = pop_int();
            long long a = pop_int();
            Value r;
            r.tag = Value::Tag::INT;
            switch (t.kind) {
            case TokKind::OP_ADD: r.i = a + b; break;
            case TokKind::OP_SUB: r.i = a - b; break;
            case TokKind::OP_MUL: r.i = a * b; break;
            case TokKind::OP_DIV:
                if (b == 0) throw std::runtime_error("division by zero on line "
                                                     + std::to_string(line));
                r.i = a / b;
                break;
            case TokKind::OP_MOD:
                if (b == 0) throw std::runtime_error("modulo by zero on line "
                                                     + std::to_string(line));
                r.i = a % b;
                break;
            case TokKind::OP_POW: {
                long long val = 1;
                long long base = a;
                long long exp = b;
                if (exp < 0) {
                    val = 0;
                }
                else {
                    for (long long k = 0; k < exp; ++k) val *= base;
                }
                r.i = val;
                break;
            }
            case TokKind::OP_EQ: r.i = (a == b) ? 1 : 0; break;
            case TokKind::OP_NE: r.i = (a != b) ? 1 : 0; break;
            case TokKind::OP_LT: r.i = (a < b)  ? 1 : 0; break;
            case TokKind::OP_LE: r.i = (a <= b) ? 1 : 0; break;
            case TokKind::OP_GT: r.i = (a > b)  ? 1 : 0; break;
            case TokKind::OP_GE: r.i = (a >= b) ? 1 : 0; break;
            case TokKind::OP_AND: r.i = (a != 0 && b != 0) ? 1 : 0; break;
            case TokKind::OP_OR:  r.i = (a != 0 || b != 0) ? 1 : 0; break;
            default: break;
            }
            stack.push_back(r);
            break;
        }
        }
    }

    if (stack.empty()) {
        Value v;
        v.tag = Value::Tag::INT;
        v.i = 0;
        return v;
    }
    return stack.back();
}

} // namespace

InterpretResult interpret(const CSTNode* program_root, std::ostream& out) {
    InterpretResult result;
    try {
        Interpreter interp(program_root, out);
        interp.run();
    }
    catch (const std::exception& e) {
        result.ok = false;
        result.error_message = e.what();
    }
    return result;
}
