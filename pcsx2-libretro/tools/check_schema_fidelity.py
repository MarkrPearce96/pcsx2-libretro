#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
# SPDX-License-Identifier: GPL-3.0+
"""
Schema-fidelity check between pcsx2-libretro's CoreOptions*.cpp and
RetroNest-Project's Pcsx2LibretroAdapter::settingsSchema().

Why this exists:
  - libretro's host (RetroNest) reconciles the user's stored options.json
    against the core's declared values list at retro_set_environment time.
    Any (key, value) pair in the host adapter that doesn't match the core's
    declared values gets silently dropped. That = silent loss of user
    settings. This script catches drift before it reaches options.json.

Usage:
  check_schema_fidelity.py
    --core <glob to CoreOptions*.cpp under pcsx2-libretro/>
    --host <path to pcsx2_libretro_adapter.cpp under RetroNest-Project/>

Exit 0 on full match; exit 1 with a diff report on any drift.
"""
import argparse
import glob
import re
import sys
from pathlib import Path


# Match an out.push_back({ ... key ... values{...} ... }) block.
# We're greedy on the top-level structure but parse the string literals
# inside. The values list is wrapped in {{...}, ...}.
CORE_BLOCK_RE = re.compile(
    r'out\.push_back\(\{\s*'
    r'"(?P<key>[^"]+)"\s*,\s*'        # key
    r'"[^"]*"\s*,\s*'                  # desc
    r'nullptr\s*,\s*'                  # desc_categorized
    r'(?:"[^"]*"\s*)+?,\s*'            # info (may span lines as adjacent string literals)
    r'nullptr\s*,\s*'                  # info_categorized
    r'nullptr\s*,\s*'                  # category_key
    r'\{(?P<values>.*?)\}\s*,\s*'      # values { {a,b}, {c,d}, ... }
    r'"(?P<default>[^"]+)"\s*,?\s*'    # default_value
    r'\}\)',
    re.DOTALL,
)

# Match each {"stored_value", "Display"} pair inside the values block.
# The terminator pair is {nullptr, nullptr} — we skip those.
VALUE_PAIR_RE = re.compile(r'\{\s*"([^"]+)"\s*,\s*"[^"]*"\s*\}')

# Host-side: s.append(opt(...)) with positional args. The current
# pcsx2_libretro_adapter.cpp uses a helper:
#   s.append(opt("pcsx2_renderer", "GS Renderer", "auto",
#                {{"Auto", "auto"}, {"Metal", "metal"}, ...},
#                "tooltip..."));
# We pull the key, the default, and the {label, stored_value} pairs from the
# initializer list (pairs in host are (label, value) — opposite order from
# core's (value, label)).
HOST_BLOCK_RE = re.compile(
    r's\.append\(\s*opt\(\s*'
    r'"(?P<key>[^"]+)"\s*,\s*'         # key
    r'"[^"]*"\s*,\s*'                  # label
    r'"(?P<default>[^"]+)"\s*,\s*'     # default value
    r'\{(?P<values>.*?)\}\s*,\s*'      # values list {{"Label", "value"}, ...}
    r'(?:"[^"]*"\s*)+'                 # tooltip (one or more adjacent string literals)
    r'\)\s*\)',
    re.DOTALL,
)

HOST_PAIR_RE = re.compile(r'\{\s*"[^"]*"\s*,\s*"([^"]+)"\s*\}')


def parse_core(paths):
    """Return {key: {"default": str, "values": set[str]}} from all matched .cpp files."""
    found = {}
    for path in paths:
        text = Path(path).read_text()
        for m in CORE_BLOCK_RE.finditer(text):
            key = m.group("key")
            default = m.group("default")
            values = {v for v in VALUE_PAIR_RE.findall(m.group("values"))}
            if key in found:
                print(f"ERROR: duplicate core key '{key}' in {path}", file=sys.stderr)
                sys.exit(1)
            found[key] = {"default": default, "values": values, "source": path}
    return found


def parse_host(path):
    """Return {key: {"default": str, "values": set[str]}}."""
    found = {}
    text = Path(path).read_text()
    for m in HOST_BLOCK_RE.finditer(text):
        key = m.group("key")
        default = m.group("default")
        values = {v for v in HOST_PAIR_RE.findall(m.group("values"))}
        # Multiple host rows can reference the same core key (Recommended
        # re-displays detailed-card rows). Merge values; flag mismatch on default.
        if key in found:
            if found[key]["default"] != default:
                print(
                    f"ERROR: host has two rows for key '{key}' with different defaults: "
                    f"'{found[key]['default']}' vs '{default}'",
                    file=sys.stderr,
                )
                sys.exit(1)
            found[key]["values"] |= values
        else:
            found[key] = {"default": default, "values": values}
    return found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--core", required=True,
                    help="glob for core CoreOptions*.cpp files")
    ap.add_argument("--host", required=True,
                    help="path to host pcsx2_libretro_adapter.cpp")
    args = ap.parse_args()

    core_paths = sorted(glob.glob(args.core))
    if not core_paths:
        print(f"ERROR: --core glob '{args.core}' matched no files", file=sys.stderr)
        return 1

    core = parse_core(core_paths)
    host = parse_host(args.host)

    if not core:
        print(f"ERROR: parsed 0 core options from {core_paths}", file=sys.stderr)
        return 1
    if not host:
        print(f"ERROR: parsed 0 host SettingDef rows from {args.host}", file=sys.stderr)
        return 1

    drift = []

    # Every host key must exist in core.
    for hkey, hrow in host.items():
        if hkey not in core:
            drift.append(f"host declares key '{hkey}' not present in core")
            continue
        crow = core[hkey]
        # Default must match.
        if hrow["default"] != crow["default"]:
            drift.append(
                f"key '{hkey}': default differs — host='{hrow['default']}' core='{crow['default']}'"
            )
        # Every host value must be in core's values list.
        missing = hrow["values"] - crow["values"]
        if missing:
            drift.append(
                f"key '{hkey}': host has values not declared in core: {sorted(missing)}"
            )
        # Every core value must be exposed by at least one host row.
        # (We only check at the per-key level — a single host row covers it.)
        unexposed = crow["values"] - hrow["values"]
        if unexposed:
            drift.append(
                f"key '{hkey}': core declares values not exposed in host: {sorted(unexposed)}"
            )

    # Every core key must appear in host (some host row, anywhere, references it).
    for ckey in core:
        if ckey not in host:
            drift.append(f"core declares key '{ckey}' with no host row")

    if drift:
        print("SCHEMA DRIFT DETECTED:", file=sys.stderr)
        for line in drift:
            print(f"  - {line}", file=sys.stderr)
        print(f"\n{len(drift)} drift entries; check both sides match exactly.", file=sys.stderr)
        return 1

    print(f"Schema fidelity OK: {len(core)} core keys, "
          f"{len(host)} host keys, byte-for-byte match.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
