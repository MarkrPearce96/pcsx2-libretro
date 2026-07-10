# pcsx2-libretro — Claude Instructions

Private PCSX2 fork with a libretro shell (`pcsx2-libretro/` subdir), loaded
in-process by RetroNest. Branch: `main`, remote `origin` =
`MarkrPearce96/pcsx2-libretro` (private, standalone — NOT a GitHub fork).

## Build + arch policy
PCSX2 is **x86_64-only** (recompilers). Local build dirs: `build-x86_64`
(the one that matters — RetroNest's daily driver is the x86_64/Rosetta app)
and `build-arm64` (stub). x86_64 CMake invocations MUST use
`arch -x86_64 /usr/local/bin/cmake` — bare `arch -x86_64 cmake` resolves to
the arm64 Homebrew cmake and dies with "Bad CPU type". Never pipe build
output (masks the exit status).

```sh
arch -x86_64 /usr/local/bin/cmake --build build-x86_64 -j 6 --target pcsx2_libretro
```

Deploy = copy `build-x86_64/.../pcsx2_libretro.dylib` to
`~/Documents/RetroNest/emulators/libretro/cores/` (plus
`pcsx2_libretro_resources/` when resources changed).

## Releases (CI)
`.github/workflows/libretro_release.yml` builds x86_64 under Rosetta on
tags (`v2026.MM.DD[.n]`). Releases are **self-contained**: dylibbundler
copies the ~11 Homebrew deps into `pcsx2_libretro_libs/` with flat
`@loader_path/<lib>` refs and ad-hoc signs — never reintroduce bare
`/usr/local/opt/...` links (that class of bug bricked in-app updates once;
an otool guard in CI enforces it).

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
