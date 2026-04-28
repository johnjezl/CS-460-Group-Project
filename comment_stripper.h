#pragma once
#include <iosfwd>
#include <string>

struct StripResult {
    bool ok = true;
    int error_line = 0;        // valid only if ok == false
    std::string error_message; // full message to print (already formatted)
};

// Strips comments from `in` and writes the result to `out`.
// Replaces comment characters with spaces and preserves newlines.
// On unterminated /* ...  prints nothing extra; returns ok=false with message+line.
StripResult strip_comments(std::istream& in, std::ostream& out);