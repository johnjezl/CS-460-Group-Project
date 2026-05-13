#pragma once

#include <iosfwd>
#include <string>

#include "cst.h"

struct InterpretResult {
    bool ok = true;
    std::string error_message;
    int line = 0;
};

InterpretResult interpret(const CSTNode* program_root, std::ostream& out);
