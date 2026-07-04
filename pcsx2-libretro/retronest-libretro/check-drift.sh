#!/usr/bin/env bash
# Verify a vendored retronest-libretro copy matches its MANIFEST.sha256
# (catches local edits — the canonical copy lives in RetroNest-Project).
# Usage: check-drift.sh [package-dir]   (defaults to this script's dir)
set -euo pipefail
cd "${1:-"$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"}"
if shasum -a 256 --check --quiet MANIFEST.sha256; then
  echo "retronest-libretro: no drift"
else
  echo "retronest-libretro: DRIFT DETECTED — do not edit vendored copies." >&2
  echo "Edit RetroNest-Project/vendor/retronest-libretro/ and re-run sync.sh." >&2
  exit 1
fi
