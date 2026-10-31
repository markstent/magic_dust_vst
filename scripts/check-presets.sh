#!/bin/sh
# Fails unless MD_COPY_PLUGIN is ON only in the dev and release presets.
python3 -c '
import json, sys
presets = json.load(open(sys.argv[1]))["configurePresets"]
bad = [p["name"] for p in presets
       if (p.get("cacheVariables", {}).get("MD_COPY_PLUGIN") == "ON") != (p["name"] in ("dev", "release"))]
if bad:
    print("MD_COPY_PLUGIN guard failed for:", ", ".join(bad))
    sys.exit(1)
' "${1:-$(dirname "$0")/../CMakePresets.json}"
