#!/usr/bin/env bash

set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_directory="$project_root/.build"

cmake -S "$project_root" -B "$build_directory" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_directory" --parallel
ctest --test-dir "$build_directory" --output-on-failure
