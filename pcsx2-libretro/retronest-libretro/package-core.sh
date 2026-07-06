#!/usr/bin/env bash
# package-core.sh — make a built libretro core self-contained.
#
# The shared implementation of deploy-contract.md §Dependency bundling
# (Packet 7 decision 5 follow-up). Previously this block lived inline —
# and drifted — in the pcsx2 and dolphin release workflows; they now call
# their vendored copy of this script. ppsspp bundles nothing (no Homebrew
# deps) and duckstation's package.sh deliberately uses an rpath-into-app-
# Frameworks strategy for its local-only deploy, so neither calls this.
#
# Usage:
#   package-core.sh <path/to/<core>_libretro.dylib> [--arch x86_64]
#
#   --arch X   run dylibbundler under `arch -X` (CI builds pcsx2/dolphin
#              x86_64 under Rosetta; the rewrite tools are arch-agnostic)
#
# Effect, in the dylib's own directory:
#   1. dylibbundler copies every non-system dep into <core>_libretro_libs/
#      and rewrites the core's load commands to @loader_path/<libs dir>/.
#   2. Sibling refs INSIDE the libs dir are flattened to
#      @loader_path/<lib>: dylibbundler applies its single prefix to every
#      rewrite, but a lib inside the dir has that dir as its own
#      @loader_path, so the prefixed ref doubles the path and the core
#      fails to dlopen (2026-07-04 field bug, fixed at source here).
#   3. Everything is ad-hoc re-signed (install_name_tool voids signatures).
#   4. Hard guard: zero /usr/local | /opt/homebrew references may remain —
#      version-drifting Homebrew links are exactly the class of bug that
#      bricked in-app updates once (rapidyaml 0.15.2 vs 0.12.1).
set -euo pipefail

ARCH_PREFIX=()
CORE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --arch) ARCH_PREFIX=(arch "-$2"); shift 2 ;;
    -h|--help) sed -n '2,27p' "$0"; exit 0 ;;
    *) CORE="$1"; shift ;;
  esac
done
if [ -z "$CORE" ] || [ ! -f "$CORE" ]; then
  echo "usage: package-core.sh <path/to/core.dylib> [--arch x86_64]" >&2
  exit 2
fi

DIR="$(cd "$(dirname "$CORE")" && pwd)"
BASE="$(basename "$CORE" .dylib)"        # e.g. pcsx2_libretro
LIBS="${BASE}_libs"
cd "$DIR"

"${ARCH_PREFIX[@]}" dylibbundler -od -b \
  -x "$BASE.dylib" \
  -d "$LIBS/" \
  -p "@loader_path/$LIBS/"

# Flatten sibling refs (step 2 above) + re-sign each bundled lib.
if [ -d "$LIBS" ]; then
  for lib in "$LIBS"/*.dylib; do
    [ -e "$lib" ] || continue
    otool -L "$lib" | tail -n +2 | awk '{print $1}' \
      | grep "@loader_path/$LIBS/" | while read -r dep; do
        install_name_tool -change "$dep" "@loader_path/$(basename "$dep")" "$lib"
      done
    codesign --force --sign - "$lib"
  done
fi
codesign --force --sign - "$BASE.dylib"

echo "=== verifying no Homebrew deps remain ==="
if otool -L "$BASE.dylib" | tail -n +2 | grep -E '/usr/local|/opt/homebrew'; then
  echo "::error::$BASE.dylib still references Homebrew paths after bundling"
  exit 1
fi
if [ -d "$LIBS" ]; then
  for lib in "$LIBS"/*.dylib; do
    [ -e "$lib" ] || continue
    if otool -L "$lib" | tail -n +2 | grep -E '/usr/local/opt|/usr/local/Cellar|/opt/homebrew'; then
      echo "::error::bundled $lib still references Homebrew"
      exit 1
    fi
  done
  echo "bundled $(ls "$LIBS" | wc -l | tr -d ' ') libs; $BASE.dylib is self-contained"
else
  echo "$BASE.dylib had no non-system deps to bundle; nothing to do"
fi
