#include "lexer.h"

#include <cctype>
#include <unordered_map>

namespace {

bool is_letter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_ident_start(char c) {
    return is_letter(c) || c == '_';
}

bool is_ident_tail(char c) {
    return is_ident_start(c) || is_digit(c);
}

bool is_hex_digit(char c) {
    return is_digit(c)
        || (c >= 'A' && c <= 'F')
        || (c >= 'a' && c <= 'f');
}

TokenType classify_identifier(const std::string& lexeme) {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"function", TokenType::FUNCTION},
        {"procedure", TokenType::PROCEDURE},
        {"int", TokenType::INT},
        {"char", TokenType::CHAR},
        {"bool", TokenType::BOOL},
        {"void", TokenType::VOID},
        {"if", TokenType::IF},
        {"else", TokenType::ELSE},
        {"while", TokenType::WHILE},
        {"for", TokenType::FOR},
        {"return", TokenType::RETURN},
        {"TRUE", TokenType::TRUE_TOKEN},
        {"FALSE", TokenType::FALSE_TOKEN},
        {"printf", TokenType::PRINTF}
    };

    auto it = keywords.find(lexeme);
    if (it != keywords.end()) {
        return it->second;
    }
    return TokenType::IDENTIFIER;
}

} // namespace

std::vector<Token> tokenize(const std::string& input, std::string& errorMsg) {
    errorMsg.clear();
    std::vector<Token> out;

    auto emit = [&](TokenType type, const std::string& lexeme, int line) {
        out.push_back(Token{type, lexeme, line});
    };

    auto fail = [&](int line, const std::string& msg) {
        errorMsg = "Syntax error on line " + std::to_string(line) + ": " + msg;
        out.clear();
    };

    std::size_t i = 0;
    int line = 1;

    while (i < input.size()) {
        char c = input[i];

        if (c == ' ' || c == '\t' || c == '\r') {
            ++i;
            continue;
        }
        if (c == '\n') {
            ++line;
            ++i;
            continue;
        }

        const int tokenStartLine = line;

        // Multi-character operators first.
        if (c == '<' && i + 1 < input.size() && input[i + 1] == '=') {
            emit(TokenType::LT_EQUAL, "<=", tokenStartLine);
            i += 2;
            continue;
        }
        if (c == '>' && i + 1 < input.size() && input[i + 1] == '=') {
            emit(TokenType::GT_EQUAL, ">=", tokenStartLine);
            i += 2;
            continue;
        }
        if (c == '&' && i + 1 < input.size() && input[i + 1] == '&') {
            emit(TokenType::BOOLEAN_AND, "&&", tokenStartLine);
            i += 2;
            continue;
        }
        if (c == '|' && i + 1 < input.size() && input[i + 1] == '|') {
            emit(TokenType::BOOLEAN_OR, "||", tokenStartLine);
            i += 2;
            continue;
        }
        if (c == '=' && i + 1 < input.size() && input[i + 1] == '=') {
            emit(TokenType::BOOLEAN_EQUAL, "==", tokenStartLine);
            i += 2;
            continue;
        }
        if (c == '!' && i + 1 < input.size() && input[i + 1] == '=') {
            emit(TokenType::BOOLEAN_NOT_EQUAL, "!=", tokenStartLine);
            i += 2;
            continue;
        }

        // Identifiers / keywords.
        if (is_ident_start(c)) {
            std::size_t start = i;
            ++i;
            while (i < input.size() && is_ident_tail(input[i])) {
                ++i;
            }
            std::string lexeme = input.substr(start, i - start);
            emit(classify_identifier(lexeme), lexeme, tokenStartLine);
            continue;
        }

        // Unsigned integer literals.
        if (is_digit(c)) {
            std::size_t start = i;
            ++i;
            while (i < input.size() && is_digit(input[i])) {
                ++i;
            }
            emit(TokenType::INTEGER, input.substr(start, i - start), tokenStartLine);
            continue;
        }

        // Double-quoted strings.
        if (c == '"') {
            ++i;
            std::string content;
            bool closed = false;

            while (i < input.size()) {
                char d = input[i];

                if (d == '"') {
                    closed = true;
                    ++i;
                    break;
                }

                if (d == '\\') {
                    if (i + 1 >= input.size()) {
                        fail(tokenStartLine, "unterminated string quote.");
                        return out;
                    }

                    if (input[i + 1] == 'x') {
                        if (i + 2 >= input.size() || !is_hex_digit(input[i + 2])) {
                            fail(tokenStartLine, "invalid escaped character");
                            return out;
                        }
                        content.push_back('\\');
                        content.push_back('x');
                        content.push_back(input[i + 2]);
                        i += 3;
                        if (i < input.size() && is_hex_digit(input[i])) {
                            content.push_back(input[i]);
                            ++i;
                        }
                        continue;
                    }

                    char esc = input[i + 1];
                    switch (esc) {
                    case 'a': case 'b': case 'f': case 'n': case 'r':
                    case 't': case 'v': case '\\': case '?': case '\'': case '"':
                        content.push_back('\\');
                        content.push_back(esc);
                        i += 2;
                        continue;
                    default:
                        fail(tokenStartLine, "invalid escaped character");
                        return out;
                    }
                }

                if (d == '\n') {
                    ++line;
                }
                content.push_back(d);
                ++i;
            }

            if (!closed) {
                fail(tokenStartLine, "unterminated string quote.");
                return out;
            }

            emit(TokenType::STRING_LITERAL, content, tokenStartLine);
            continue;
        }

        // Single-quoted strings / char literals.
        if (c == '\'') {
            ++i;
            std::string content;
            bool closed = false;

            while (i < input.size()) {
                char d = input[i];

                if (d == '\'') {
                    closed = true;
                    ++i;
                    break;
                }

                if (d == '\\') {
                    if (i + 1 >= input.size()) {
                        fail(tokenStartLine, "unterminated character quote.");
                        return out;
                    }

                    if (input[i + 1] == 'x') {
                        if (i + 2 >= input.size() || !is_hex_digit(input[i + 2])) {
                            fail(tokenStartLine, "invalid escaped character");
                            return out;
                        }
                        content.push_back('\\');
                        content.push_back('x');
                        content.push_back(input[i + 2]);
                        i += 3;
                        if (i < input.size() && is_hex_digit(input[i])) {
                            content.push_back(input[i]);
                            ++i;
                        }
                        continue;
                    }

                    char esc = input[i + 1];
                    switch (esc) {
                    case 'a': case 'b': case 'f': case 'n': case 'r':
                    case 't': case 'v': case '\\': case '?': case '\'': case '"':
                        content.push_back('\\');
                        content.push_back(esc);
                        i += 2;
                        continue;
                    default:
                        fail(tokenStartLine, "invalid escaped character");
                        return out;
                    }
                }

                if (d == '\n') {
                    ++line;
                }
                content.push_back(d);
                ++i;
            }

            if (!closed) {
                fail(tokenStartLine, "unterminated character quote.");
                return out;
            }

            emit(TokenType::CHAR_LITERAL, content, tokenStartLine);
            continue;
        }

        // Single-character punctuation / operators.
        switch (c) {
        case '(': emit(TokenType::L_PAREN, "(", tokenStartLine); ++i; continue;
        case ')': emit(TokenType::R_PAREN, ")", tokenStartLine); ++i; continue;
        case '[': emit(TokenType::L_BRACKET, "[", tokenStartLine); ++i; continue;
        case ']': emit(TokenType::R_BRACKET, "]", tokenStartLine); ++i; continue;
        case '{': emit(TokenType::L_BRACE, "{", tokenStartLine); ++i; continue;
        case '}': emit(TokenType::R_BRACE, "}", tokenStartLine); ++i; continue;
        case ';': emit(TokenType::SEMICOLON, ";", tokenStartLine); ++i; continue;
        case ',': emit(TokenType::COMMA, ",", tokenStartLine); ++i; continue;
        case '=': emit(TokenType::ASSIGNMENT_OPERATOR, "=", tokenStartLine); ++i; continue;
        case '+': emit(TokenType::PLUS, "+", tokenStartLine); ++i; continue;
        case '-': emit(TokenType::MINUS, "-", tokenStartLine); ++i; continue;
        case '*': emit(TokenType::ASTERISK, "*", tokenStartLine); ++i; continue;
        case '/': emit(TokenType::DIVIDE, "/", tokenStartLine); ++i; continue;
        case '%': emit(TokenType::MODULO, "%", tokenStartLine); ++i; continue;
        case '^': emit(TokenType::CARET, "^", tokenStartLine); ++i; continue;
        case '<': emit(TokenType::LT, "<", tokenStartLine); ++i; continue;
        case '>': emit(TokenType::GT, ">", tokenStartLine); ++i; continue;
        case '!': emit(TokenType::BOOLEAN_NOT, "!", tokenStartLine); ++i; continue;
        default:
            fail(tokenStartLine, std::string("unexpected character '") + c + "'");
            return out;
        }
    }

    emit(TokenType::END_OF_FILE, "", line);
    return out;
}
