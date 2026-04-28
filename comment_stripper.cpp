#include "comment_stripper.h"

#include <iostream> 
#include <string>

namespace {

    enum class State {
        NORMAL,
        SAW_SLASH,     // saw '/'
        LINE_COMMENT,  // after //
        BLOCK_COMMENT, // after /*
        BLOCK_STAR,    // in block comment and just saw '*'
        DQUOTE,        // inside "..."
        DQUOTE_ESC,    // after backslash inside "..."
        SQUOTE,        // inside '...'
        SQUOTE_ESC,    // after backslash inside '...'
        SAW_STAR       // saw '*' in NORMAL (possible stray "*/")
    };

    inline void put_space_or_newline(std::ostream& out, char c) {
        if (c == '\n') out.put('\n');
        else          out.put(' ');
    }

} // namespace

StripResult strip_comments(std::istream& in, std::ostream& out) {
    StripResult res;

    State st = State::NORMAL;
    int line = 1;
    int blockStartLine = -1;

    int ich;
    while ((ich = in.get()) != EOF) {
        char c = static_cast<char>(ich);

        switch (st) {
        case State::NORMAL:
            if (c == '/') {
                st = State::SAW_SLASH;
            }
            else if (c == '*') {
                st = State::SAW_STAR;
            }
            else if (c == '"') {
                out.put(c);
                st = State::DQUOTE;
            }
            else if (c == '\'') {
                out.put(c);
                st = State::SQUOTE;
            }
            else {
                out.put(c);
            }
            break;

        case State::SAW_SLASH:
            if (c == '/') {
                // "//" -> spaces
                out.put(' ');
                out.put(' ');
                st = State::LINE_COMMENT;
            }
            else if (c == '*') {
                // "/*" -> spaces
                out.put(' ');
                out.put(' ');
                st = State::BLOCK_COMMENT;
                blockStartLine = line;
            }
            else {
                // not a comment: output held '/', then re-handle c
                out.put('/');

                if (c == '/') {
                    st = State::SAW_SLASH;
                }
                else if (c == '*') {
                    st = State::SAW_STAR;
                }
                else if (c == '"') {
                    out.put(c);
                    st = State::DQUOTE;
                }
                else if (c == '\'') {
                    out.put(c);
                    st = State::SQUOTE;
                }
                else {
                    out.put(c);
                    st = State::NORMAL;
                }
            }
            break;

        case State::LINE_COMMENT:
            if (c == '\n') {
                out.put('\n');
                st = State::NORMAL;
            }
            else {
                out.put(' ');
            }
            break;

        case State::BLOCK_COMMENT:
            if (c == '*') {
                out.put(' ');
                st = State::BLOCK_STAR;
            }
            else {
                put_space_or_newline(out, c);
            }
            break;

        case State::BLOCK_STAR:
            if (c == '/') {
                // end of block comment "*/"
                out.put(' ');
                st = State::NORMAL;
            }
            else if (c == '*') {
                out.put(' ');
                st = State::BLOCK_STAR;
            }
            else {
                put_space_or_newline(out, c);
                st = State::BLOCK_COMMENT;
            }
            break;

        case State::DQUOTE:
            out.put(c);
            if (c == '\\') st = State::DQUOTE_ESC;
            else if (c == '"') st = State::NORMAL;
            break;

        case State::DQUOTE_ESC:
            out.put(c);
            st = State::DQUOTE;
            break;

        case State::SQUOTE:
            out.put(c);
            if (c == '\\') st = State::SQUOTE_ESC;
            else if (c == '\'') st = State::NORMAL;
            break;

        case State::SQUOTE_ESC:
            out.put(c);
            st = State::SQUOTE;
            break;

        case State::SAW_STAR:
            if (c == '/') {
                // Stray "*/" outside a block comment:
                // Replace it with whitespace and continue silently (do NOT print warning).
                out.put(' ');
                out.put(' ');
                st = State::NORMAL;
            }
            else {
                // not "*/": output held '*', then re-handle c
                out.put('*');

                if (c == '/') {
                    st = State::SAW_SLASH;
                }
                else if (c == '*') {
                    st = State::SAW_STAR;
                }
                else if (c == '"') {
                    out.put(c);
                    st = State::DQUOTE;
                }
                else if (c == '\'') {
                    out.put(c);
                    st = State::SQUOTE;
                }
                else {
                    out.put(c);
                    st = State::NORMAL;
                }
            }
            break;
        }

        if (c == '\n') line++;
    }

    // EOF cleanup for delayed states
    if (st == State::SAW_SLASH) {
        out.put('/');
        st = State::NORMAL;
    }
    if (st == State::SAW_STAR) {
        out.put('*');
        st = State::NORMAL;
    }

    // Unterminated block comment
    if (st == State::BLOCK_COMMENT || st == State::BLOCK_STAR) {
        res.ok = false;
        res.error_line = blockStartLine;
        res.error_message =
            "ERROR: Program contains C-style, unterminated comment on line " +
            std::to_string(blockStartLine);
        return res;
    }

    return res;
}