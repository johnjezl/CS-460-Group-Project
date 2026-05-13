#!/usr/bin/env bash
# Build the interpreter and run all PA6 test programs.
# Compares each program's output against the expected output file
# in test-data/ and reports PASS / FAIL.

set -u

BIN=./assignment
TEST_DIR=test-data
TESTS=(1 2 3)

if [ ! -x "$BIN" ] && [ ! -x "$BIN.exe" ]; then
    echo "Building..."
    make || { echo "build failed"; exit 1; }
fi

# On Windows the produced binary is assignment.exe; fall back to it.
if [ ! -x "$BIN" ] && [ -x "$BIN.exe" ]; then
    BIN="$BIN.exe"
fi

pass=0
fail=0
for n in "${TESTS[@]}"; do
    input="$TEST_DIR/programming_assignment_6-test_file_$n.c"
    expected="$TEST_DIR/output-programming_assignment_6-test_program_$n.txt"
    actual="$TEST_DIR/actual_$n.txt"

    if [ ! -f "$input" ]; then
        echo "TEST $n: SKIP (missing $input)"
        continue
    fi
    if [ ! -f "$expected" ]; then
        echo "TEST $n: SKIP (missing $expected)"
        continue
    fi

    "$BIN" "$input" > "$actual"

    if diff -q "$actual" "$expected" > /dev/null 2>&1; then
        echo "TEST $n: PASS"
        pass=$((pass + 1))
        rm -f "$actual"
    else
        echo "TEST $n: FAIL"
        echo "--- diff (actual vs expected) ---"
        diff "$actual" "$expected" || true
        echo "---------------------------------"
        fail=$((fail + 1))
    fi
done

echo
echo "Summary: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
