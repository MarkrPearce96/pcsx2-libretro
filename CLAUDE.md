# pcsx2-libretro — Claude Instructions

Private PCSX2 fork with a libretro shell (`pcsx2-libretro/` subdir), loaded
in-process by RetroNest. Branch: `main`, remote `origin` =
`MarkrPearce96/pcsx2-libretro` (private, standalone — NOT a GitHub fork).

## Build + arch policy
Two architectures, one tree (branch `arm64-merge`; `main` is still x86-only):
- **x86_64** (upstream recompilers): local dir `build-x86_64`. CMake MUST use
  `arch -x86_64 /usr/local/bin/cmake` — bare `arch -x86_64 cmake` resolves to
  the arm64 Homebrew cmake and dies with "Bad CPU type".
- **arm64 native** (ARMSX2 recompilers, imported 2026-07-18): local dir
  `build-arm64`. Use /opt/homebrew tools + prefix, and BOTH
  `-DCMAKE_IGNORE_PATH=/usr/local -DCMAKE_IGNORE_PREFIX_PATH=/usr/local`:

```sh
arch -x86_64 /usr/local/bin/cmake --build build-x86_64 -j 6 --target pcsx2_libretro
# arm64:
PATH=/opt/homebrew/bin:$PATH cmake -S . -B build-arm64 -G Ninja \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DCMAKE_IGNORE_PATH=/usr/local -DCMAKE_IGNORE_PREFIX_PATH=/usr/local
cmake --build build-arm64 --target pcsx2_libretro -j
```

Never pipe build output (masks the exit status). At runtime the core needs
`pcsx2_libretro_resources/` NEXT TO the dylib (dladdr-resolved), and the
`.metallib`s inside it MUST be rebuilt whenever the PCSX2 base moves —
stale ones assert in GSDeviceMTL (function-constant type mismatch). Local
standalone testing: `pcsx2-libretro/tools/test_boot_macos.mm` (see its
header; needs codesign with allow-jit for the arm64 MAP_JIT recompilers).

Deploy = copy `build-x86_64/.../pcsx2_libretro.dylib` to
`~/Documents/RetroNest/emulators/libretro/cores/` (plus
`pcsx2_libretro_resources/` when resources changed).

## Releases (CI)
`.github/workflows/libretro_release.yml` publishes ONE **UNIVERSAL** macOS
zip on tags (`v2026.MM.DD[.n]`), per the standing all-cores-universal policy:
parallel x86_64 (Rosetta, /usr/local brew) + arm64 (native, /opt/homebrew,
ARMSX2 recompilers) build jobs → per-arch dep bundles → a macos merge job
lipos dylib + the ~11 bundled libs pairwise, re-signs, and verifies (both
slices, no Homebrew refs, lib-set parity guard) → `pcsx2_libretro.dylib.zip`
(historical name; VERSION platform=macos-universal). `workflow_dispatch`
runs a publish-free dry-run (release job is tag-gated). Releases are
**self-contained**: never reintroduce bare `/usr/local/opt/...` or
`/opt/homebrew/...` links (that class of bug bricked in-app updates once;
otool guards in CI enforce it). Metallibs are compiled inline in the x86
job — they MUST be rebuilt whenever the PCSX2 base moves (stale ones assert
in GSDeviceMTL). Releases are currently cut from the `arm64-merge` branch
(`main` still holds the pre-ARMSX2 May x86 base until the merge). A future
Windows port ships a separate win-x86_64 asset — this workflow is macOS-only.

## RetroNest contract package
`pcsx2-libretro/retronest-libretro/` is a VENDORED COPY of
`RetroNest-Project/vendor/retronest-libretro/`. NEVER edit it here — edit
the canonical package and run its `sync.sh`; the build fails on checksum
drift (`check-drift.sh` + `MANIFEST.sha256`).

## Settings options
RetroNest renders its PCSX2 settings pages FROM this core's declared
options (`CoreOptions.cpp` → `SET_CORE_OPTIONS_V2`). Changing option
keys/values/defaults here flows into RetroNest automatically after a
rebuild + re-probe; follow `retronest-libretro/docs/option-style-guide.md`.

## Updating from upstream (carries patches — a sync is real work)
Unlike the stock `mgba-libretro` mirror, this fork carries RetroNest source
patches (NSView/Metal handoff, the `RETRONEST_ENVIRONMENT_*` contract,
CoreOptions, the WorkSema deadlock band-aid), so an upstream sync **can and
will conflict**. `upstream` = `PCSX2/pcsx2`, branch `main`, release arch
**x86_64** (CI builds under Rosetta).
```sh
git fetch upstream
git merge upstream/master        # resolve conflicts where upstream touched
                                 # the same code as our patches
# if the contract package changed, re-sync from RetroNest-Project:
#   ./vendor/retronest-libretro/sync.sh   (build fails on drift otherwise)
# REBUILD LOCALLY + TEST IN RETRONEST — not just "compiles": confirm rendering
# (NSView handoff), 2-player, analog/rumble, settings schema still work.
git push origin main
git tag v2026.MM.DD && git push origin v2026.MM.DD   # CI rebuilds + republishes
```
Only sync when you actually want an upstream fix/feature — each sync costs
conflict-resolution + a full retest. Upstreaming patches (below) shrinks the
delta and makes future syncs easier.

## Upstream PRs
Upstream PRs go through the TRUE fork `prfork` = `MarkrPearce96/pcsx2`
(e.g. #14658, WorkSema). Pushing upstream-derived branches to THIS repo
triggers upstream's CI workflows here — cancel those runs.
