#!/usr/bin/env bash
# Runs pluginval (VST3 and AU) and auval on the built Magic Dust plugin.
# Usage: scripts/validate.sh [build-dir]   (default build/dev)
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${1:-$root/build/dev}"
pluginval="${PLUGINVAL:-$HOME/Applications/pluginval.app/Contents/MacOS/pluginval}"
config="$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$build_dir/CMakeCache.txt" 2>/dev/null || true)"
artefacts="$build_dir/plugin/MagicDust_artefacts/$config"

if [ -z "$config" ] || [ ! -d "$artefacts" ]; then
  echo "validate.sh: expected built plugins in '$artefacts' (CMAKE_BUILD_TYPE='$config'); build first with cmake --build --preset dev" >&2
  exit 1
fi

for bundle in "$artefacts/VST3/Magic Dust.vst3" "$artefacts/AU/Magic Dust.component"; do
  "$pluginval" --strictness-level 10 --validate-in-process --skip-gui-tests --validate "$bundle"
done
auval -v aumu Mdst Mdus
