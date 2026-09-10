#!/usr/bin/env bash
# Apply (or check) clang-format across the project.
#
#   scripts/format.sh          rewrite files in place
#   scripts/format.sh --check  fail if anything is unformatted (used by CI)
#
# The formatter is the binary pinned in package.json, so a developer machine and
# CI produce byte-identical output. Different clang-format versions disagree on
# real code, which turns the check into noise. Run `npm install` once to fetch
# it. CLANG_FORMAT overrides the choice if you know what you are doing.

set -euo pipefail

cd "$(dirname "$0")/.."

find_clang_format() {
    if [ -n "${CLANG_FORMAT:-}" ]; then
        echo "$CLANG_FORMAT"
        return
    fi

    local base="node_modules/clang-format/bin"
    local candidate
    for candidate in \
        "$base/win32/clang-format.exe" \
        "$base/linux_x64/clang-format" \
        "$base/darwin_x64/clang-format"
    do
        if [ -x "$candidate" ]; then
            echo "$candidate"
            return
        fi
    done

    # Fall back to whatever is installed, accepting the version risk.
    if command -v clang-format >/dev/null 2>&1; then
        echo "clang-format"
        return
    fi

    return 1
}

if ! CLANG_FORMAT_BIN="$(find_clang_format)"; then
    echo "error: no clang-format found. Run 'npm install' to fetch the pinned one." >&2
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

echo "using $("$CLANG_FORMAT_BIN" --version)"

if [ "${1:-}" = "--check" ]; then
    "$CLANG_FORMAT_BIN" --dry-run --Werror -style=file "${files[@]}"
    echo "formatting OK (${#files[@]} files)"
else
    "$CLANG_FORMAT_BIN" -i -style=file "${files[@]}"
    echo "formatted ${#files[@]} files"
fi
