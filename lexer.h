#pragma once
#include <string>
#include <vector>
#include "tokens.h"

// Tokenize a comment-stripped program (comments already replaced with whitespace).
// Returns tokens; on error returns empty vector and sets errorMsg.
std::vector<Token> tokenize(const std::string& input, std::string& errorMsg);