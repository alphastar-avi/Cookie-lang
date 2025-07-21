#!/bin/bash

# Test script for Cookie compiler examples

set -e

echo "Testing Cookie Compiler examples..."

# Test simple loop example
echo "Testing cookie.cook (simple loop)..."
./bin/cookie_compiler.sh examples/cookie.cook
echo "Running executable:"
./a.out
echo ""

# Test input/variables example
echo "Testing lang.cook (variables and input)..."
./bin/cookie_compiler.sh -o lang_test examples/lang.cook
echo "Running executable (input: 10):"
echo "10" | ./lang_test
echo ""

echo "All examples tested successfully!"
