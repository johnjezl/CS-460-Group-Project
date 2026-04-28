#include "symbol_table.h"

#include <iostream>
#include <ostream>

SemanticError::SemanticError(int line, const std::string& message)
    : std::runtime_error("Error on line " + std::to_string(line) + ": " + message),
    line_(line),
    plainMessage_(message) {
}

int SemanticError::line() const noexcept {
    return line_;
}

const std::string& SemanticError::plain_message() const noexcept {
    return plainMessage_;
}

SymbolTable::SymbolTable()
    : head_(nullptr), tail_(nullptr) {
}

SymbolTable::~SymbolTable() {
    SymbolNode* cur = head_;
    while (cur != nullptr) {
        SymbolNode* next = cur->next;
        destroy_parameters(cur->paramHead);
        delete cur;
        cur = next;
    }
}

void SymbolTable::destroy_parameters(ParameterNode* head) {
    while (head != nullptr) {
        ParameterNode* next = head->next;
        delete head;
        head = next;
    }
}

void SymbolTable::append_symbol(SymbolNode* node) {
    if (head_ == nullptr) {
        head_ = node;
        tail_ = node;
    }
    else {
        tail_->next = node;
        tail_ = node;
    }
}

SymbolNode* SymbolTable::add_variable(const std::string& name,
    const std::string& datatype,
    bool isArray,
    int arraySize,
    int scope,
    int line) {
    SymbolNode* node = new SymbolNode;
    node->name = name;
    node->kind = SymbolKind::DATATYPE;
    node->datatype = datatype;
    node->isArray = isArray;
    node->arraySize = arraySize;
    node->scope = scope;
    node->line = line;

    append_symbol(node);
    return node;
}

SymbolNode* SymbolTable::add_function(const std::string& name,
    const std::string& returnType,
    int scope,
    int line) {
    SymbolNode* node = new SymbolNode;
    node->name = name;
    node->kind = SymbolKind::FUNCTION;
    node->datatype = returnType;
    node->isArray = false;
    node->arraySize = 0;
    node->scope = scope;
    node->line = line;

    append_symbol(node);
    return node;
}

SymbolNode* SymbolTable::add_procedure(const std::string& name,
    int scope,
    int line) {
    SymbolNode* node = new SymbolNode;
    node->name = name;
    node->kind = SymbolKind::PROCEDURE;
    node->datatype = "NOT APPLICABLE";
    node->isArray = false;
    node->arraySize = 0;
    node->scope = scope;
    node->line = line;

    append_symbol(node);
    return node;
}

ParameterNode* SymbolTable::add_parameter(SymbolNode* owner,
    const std::string& name,
    const std::string& datatype,
    bool isArray,
    int arraySize,
    int scope,
    int line) {
    if (owner == nullptr) {
        return nullptr;
    }

    ParameterNode* param = new ParameterNode;
    param->name = name;
    param->datatype = datatype;
    param->isArray = isArray;
    param->arraySize = arraySize;
    param->scope = scope;
    param->line = line;

    if (owner->paramHead == nullptr) {
        owner->paramHead = param;
        owner->paramTail = param;
    }
    else {
        owner->paramTail->next = param;
        owner->paramTail = param;
    }

    return param;
}

SymbolNode* SymbolTable::find_global(const std::string& name) const {
    for (SymbolNode* cur = head_; cur != nullptr; cur = cur->next) {
        if (cur->scope == 0 && cur->name == name) {
            return cur;
        }
    }
    return nullptr;
}

SymbolNode* SymbolTable::find_in_scope(const std::string& name, int scope) const {
    for (SymbolNode* cur = head_; cur != nullptr; cur = cur->next) {
        if (cur->scope == scope && cur->name == name) {
            return cur;
        }
    }
    return nullptr;
}

bool SymbolTable::exists_in_global_scope(const std::string& name) const {
    return find_global(name) != nullptr;
}

bool SymbolTable::parameter_exists(const SymbolNode* owner, const std::string& name) const {
    if (owner == nullptr) {
        return false;
    }

    for (ParameterNode* cur = owner->paramHead; cur != nullptr; cur = cur->next) {
        if (cur->name == name) {
            return true;
        }
    }
    return false;
}

bool SymbolTable::exists_in_local_scope(const SymbolNode* owner, const std::string& name) const {
    if (owner == nullptr) {
        return false;
    }

    if (parameter_exists(owner, name)) {
        return true;
    }

    for (SymbolNode* cur = head_; cur != nullptr; cur = cur->next) {
        if (cur->scope == owner->scope &&
            cur->kind == SymbolKind::DATATYPE &&
            cur->name == name) {
            return true;
        }
    }

    return false;
}

const char* SymbolTable::kind_text(SymbolKind kind) {
    switch (kind) {
    case SymbolKind::DATATYPE:
        return "datatype";
    case SymbolKind::FUNCTION:
        return "function";
    case SymbolKind::PROCEDURE:
        return "procedure";
    }
    return "";
}

const char* SymbolTable::yes_no(bool value) {
    return value ? "yes" : "no";
}

void SymbolTable::print_symbol(std::ostream& out, const SymbolNode& sym) {
    out << "      IDENTIFIER_NAME: " << sym.name << "\n";
    out << "      IDENTIFIER_TYPE: " << kind_text(sym.kind) << "\n";
    out << "             DATATYPE: " << sym.datatype << "\n";
    out << "    DATATYPE_IS_ARRAY: " << yes_no(sym.isArray) << "\n";
    out << "  DATATYPE_ARRAY_SIZE: " << sym.arraySize << "\n";
    out << "                SCOPE: " << sym.scope << "\n\n";
}

void SymbolTable::print_parameter(std::ostream& out, const ParameterNode& param) {
    out << "      IDENTIFIER_NAME: " << param.name << "\n";
    out << "             DATATYPE: " << param.datatype << "\n";
    out << "    DATATYPE_IS_ARRAY: " << yes_no(param.isArray) << "\n";
    out << "  DATATYPE_ARRAY_SIZE: " << param.arraySize << "\n";
    out << "                SCOPE: " << param.scope << "\n\n";
}

void SymbolTable::print(std::ostream& out) const {
    for (SymbolNode* cur = head_; cur != nullptr; cur = cur->next) {
        print_symbol(out, *cur);
    }

    bool printedAnyParameterSection = false;

    for (SymbolNode* cur = head_; cur != nullptr; cur = cur->next) {
        if (cur->paramHead == nullptr) {
            continue;
        }

        if (!printedAnyParameterSection) {
            out << "\n";
            printedAnyParameterSection = true;
        }
        else {
            out << "\n";
        }

        out << "   PARAMETER LIST FOR: " << cur->name << "\n";

        for (ParameterNode* p = cur->paramHead; p != nullptr; p = p->next) {
            print_parameter(out, *p);
        }
    }
}