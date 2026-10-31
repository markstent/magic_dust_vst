#!/usr/bin/env bash
# Fails if any C++ source under the project directories is not clang-formatted.
# Usage: scripts/format-check.sh   (clang-format path from CLANG_FORMAT)
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
clang_format="${CLANG_FORMAT:-/opt/homebrew/opt/llvm/bin/clang-format}"

dirs=()
for d in engine bank midiexport plugin ui tools tests; do
  [ -d "$root/$d" ] && dirs+=("$root/$d")
done

find "${dirs[@]}" -name .venv -prune -o -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) -print0 |
  xargs -0 "$clang_format" --dry-run --Werror
