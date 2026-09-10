#!/usr/bin/env bash
# Normalize specification markdown imported with HTML entities and backslash
# escapes left intact.
#
#   1. &#x20;          -> literal space (used as leading indent in ASCII diagrams)
#   2. \. \* \_ \~ ... -> unescaped punctuation
#   3. blank lines that split an ASCII diagram into separate paragraphs -> dropped
#   4. runs of blank lines -> a single blank line
#
# Rule for (3): a blank run is dropped when the next non-blank line is a
# "diagram" line, i.e. it is indented or opens with a box-drawing character.
# Otherwise one blank line is kept as a paragraph separator.
#
# Idempotent. Rewrites files in place; review the result with `git diff`.

set -euo pipefail

[ $# -ge 1 ] || { echo "usage: $0 <file.md> [file.md ...]" >&2; exit 2; }

for f in "$@"; do
  [ -f "$f" ] || { echo "skip (not a file): $f" >&2; continue; }

  tmp="$(mktemp)"

  # An escaped backslash is parked on \x01 so the generic unescape below does
  # not consume it, then restored afterwards.
  sed -e 's/&#x20;/ /g' \
      -e 's/\\\\/\x01/g' \
      -e 's/\\\([[:punct:]]\)/\1/g' \
      -e 's/\x01/\\/g' \
      "$f" \
  | awk '
      function is_diag(s) {
        return (s ~ /^[ \t]/) ||
               (s ~ /^[|+\/\\^v<>-]/) ||
               (s ~ /^(├|└|│|─|┌|┐|┘|┬|┴|┼|╔|╗|╚|╝|═|║|▼|▲|►|◄|→|←|↑|↓)/)
      }
      { line[NR] = $0 }
      END {
        for (i = 1; i <= NR; i++) {
          if (line[i] ~ /^[ \t]*$/) {
            j = i
            while (j <= NR && line[j] ~ /^[ \t]*$/) j++
            if (j > NR) break                       # trailing blank run: drop
            if (emitted > 0 && !is_diag(line[j])) print ""
            i = j - 1
            continue
          }
          print line[i]
          emitted++
        }
      }
    ' > "$tmp"

  mv "$tmp" "$f"
  echo "normalized: $f"
done
