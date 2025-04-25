#!/bin/bash

# Recursively format all .c and .h files from current directory
find . -type f \( -name "*.c" -o -name "*.h" \) -exec clang-format -i {} +

echo "✅ All .c and .h files formatted with clang-format."
