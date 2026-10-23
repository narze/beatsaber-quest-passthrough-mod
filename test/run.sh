#!/usr/bin/env bash
# Build and run the host unit tests (no headset needed).
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-test
for t in test/*_test.cpp; do
  name=$(basename "$t" .cpp)
  c++ -std=c++20 -Wall -Wextra -Werror -Iinclude "$t" -o "build-test/$name"
  "./build-test/$name"
done
