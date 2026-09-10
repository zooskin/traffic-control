#!/usr/bin/env bash
# Apply (or check) clang-format across the project.
#
#   scripts/format.sh          rewrite files in place
#   scripts/format.sh --check  fail if anything is unformatted (used by CI)

set -euo pipefail

cd "$(dirname "$0")/.."

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    echo "error: $CLANG_FORMAT not found. Set CLANG_FORMAT to its path." >&2
    exit 1
fi

mapfile -t files < <(
    find include src tests simulation benchmarks \
        \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) 2>/dev/null | sort
)

if [ ${#files[@]} -eq 0 ]; then
    echo "no sources to format"
    exit 0
fi

if [ "${1:-}" = "--check" ]; then
    "$CLANG_FORMAT" --dry-run --Werror "${files[@]}"
    echo "formatting OK (${#files[@]} files)"
else
    "$CLANG_FORMAT" -i "${files[@]}"
    echo "formatted ${#files[@]} files"
fi
