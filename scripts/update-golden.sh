#!/usr/bin/env bash
# Regenerates tests/golden for review. Never commits.
# Usage: scripts/update-golden.sh   (works from any folder)
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
review="$root/build/golden-review"

cd "$root"
cmake --preset dev >/dev/null
cmake --build --preset dev --target md_golden_tests

rm -rf "$review"
mkdir -p "$review/before" "$review/after"
find "$root/tests/golden" -maxdepth 1 -name '*.mid' -exec cp {} "$review/before/" \;

MD_UPDATE_GOLDEN=1 "$root/build/dev/tests/md_golden_tests" >/dev/null

find "$root/tests/golden" -maxdepth 1 -name '*.mid' -exec cp {} "$review/after/" \;

changed=0
for after in "$review"/after/*.mid; do
  name="$(basename "$after")"
  if [ ! -f "$review/before/$name" ] || ! cmp -s "$review/before/$name" "$after"; then
    echo "changed: $name"
    changed=$((changed + 1))
  fi
done
echo "$changed case(s) changed"

echo "Listen to before/ and after/ in Live. If you approve, commit tests/golden. If not, run git checkout tests/golden."
echo "Files are in $review"
