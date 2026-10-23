#!/usr/bin/env bash
# Build and run the host unit tests (no headset needed).
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build-test
c++ -std=c++20 -Wall -Wextra -Werror -Iinclude test/xr_logic_test.cpp -o build-test/xr_logic_test
./build-test/xr_logic_test
