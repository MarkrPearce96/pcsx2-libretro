# retronest-libretro — the RetroNest ⇄ core contract package

This directory is the CANONICAL copy of everything shared between the
RetroNest frontend and its libretro core forks:

| File | What |
|---|---|
| `libretro.h` | Pinned upstream libretro ABI header (single vintage suite-wide) |
| `retronest_libretro.h` | Private env-command registry, game-identity struct, optional-export prototypes |
| `retronest_nsview.h/.mm` | Main-thread-safe NSView metrics query (Metal cores) |
| `emit_core_options_v2.h` | Shared SET_CORE_OPTIONS_V2 emission |
| `docs/option-style-guide.md` | Grammar rules for NEW core options |
| `docs/deploy-contract.md` | Core packaging/deploy contract (zip layout, signing, paths) |

## Editing workflow

1. Edit files HERE (never in a fork's vendored copy).
2. Run `./sync.sh` — regenerates `MANIFEST.sha256` and copies the package
   into every adopting fork.
3. Rebuild + smoke-test each fork you re-synced.

Each fork verifies its copy against `MANIFEST.sha256` on every build
(`retronest_contract_check` CMake target running `check-drift.sh`), so a
locally edited vendored copy fails that fork's build.

## Adopters

- RetroNest-Project (canonical, via `cpp/CMakeLists.txt` include dirs)
- duckstation-libretro → `src/duckstation-libretro/retronest-libretro/`
- pcsx2-libretro → `pcsx2-libretro/retronest-libretro/`
- dolphin-libretro → `Source/Core/DolphinLibretro/retronest-libretro/`
- ppsspp-libretro: NOT yet (its `ppsspp-libretro/` scaffold has no code)
- mgba: NEVER while it ships as a stock upstream core

Each fork also keeps a 2-line forwarding `libretro.h` at its historical
header path so upstream-era `#include "libretro.h"` lines keep resolving.

## Refreshing the pinned libretro.h

    curl -fsSL -o libretro.h \
      https://raw.githubusercontent.com/libretro/RetroArch/master/libretro-common/include/libretro.h

then re-add the RetroNest banner block at the top, run `./sync.sh`, and
rebuild ALL adopters (an ABI-header bump is a suite-wide event).
