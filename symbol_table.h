#pragma once
#pragma once

#include <iosfwd>
#include <stdexcept>
#include <string>

enum class SymbolKind {
    DATATYPE,
    FUNCTION,
    PROCEDURE
};

class SemanticError : public std::runtime_error {
public:
    SemanticError(int line, const std::string& message);

    int line() const noexcept;
    const std::string& plain_message() const noexcept;

private:
    int line_;
    std::string plainMessage_;
};

struct ParameterNode {
    std::string name;
    std::string datatype;
    bool isArray = false;
    int arraySize = 0;
    int scope = 0;
    int line = 0;

    ParameterNode* next = nullptr;
};

struct SymbolNode {
    std::string name;
    SymbolKind kind = SymbolKind::DATATYPE;
    std::string datatype;      // int, char, bool, or "NOT APPLICABLE"
    bool isArray = false;
    int arraySize = 0;
    int scope = 0;
    int line = 0;

    ParameterNode* paramHead = nullptr;
    ParameterNode* paramTail = nullptr;

    SymbolNode* next = nullptr;
};

class SymbolTable {
public:
    SymbolTable();
    ~SymbolTable();

    SymbolTable(const SymbolTable&) = delete;
    SymbolTable& operator=(const SymbolTable&) = delete;

    SymbolNode* add_variable(const std::string& name,
        const std::string& datatype,
        bool isArray,
        int arraySize,
        int scope,
        int line);

    SymbolNode* add_function(const std::string& name,
        const std::string& returnType,
        int scope,
        int line);

    SymbolNode* add_procedure(const std::string& name,
        int scope,
        int line);

    ParameterNode* add_parameter(SymbolNode* owner,
        const std::string& name,
        const std::string& datatype,
        bool isArray,
        int arraySize,
        int scope,
        int line);

    // Lookup helpers for duplicate checks
    SymbolNode* find_global(const std::string& name) const;
    SymbolNode* find_in_scope(const std::string& name, int scope) const;

    bool exists_in_global_scope(const std::string& name) const;
    bool exists_in_local_scope(const SymbolNode* owner, const std::string& name) const;
    bool parameter_exists(const SymbolNode* owner, const std::string& name) const;

    void print(std::ostream& out) const;

private:
    SymbolNode* head_;
    SymbolNode* tail_;

    void append_symbol(SymbolNode* node);

    static void destroy_parameters(ParameterNode* head);
    static const char* kind_text(SymbolKind kind);
    static const char* yes_no(bool value);

    static void print_symbol(std::ostream& out, const SymbolNode& sym);
    static void print_parameter(std::ostream& out, const ParameterNode& param);
};