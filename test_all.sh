#!/bin/bash
set -e

echo "🔄 [1/3] Debug build + tests"
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

echo ""
echo "🧪 [2/3] AddressSanitizer build + tests"
cmake --preset asan
cmake --build --preset asan
ctest --preset asan

echo ""
echo "🔒 [3/3] ThreadSanitizer build + tests"
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan

echo ""
echo "✅ All tests passed in Debug, ASAN, and TSAN modes."
