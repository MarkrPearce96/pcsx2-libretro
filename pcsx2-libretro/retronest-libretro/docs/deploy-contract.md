# Core deploy/packaging contract

The single description of how a RetroNest core reaches
`{root}/emulators/libretro/cores/`. The §Dependency bundling steps are
implemented ONCE in this package's `package-core.sh` — the pcsx2 and
dolphin release workflows call their vendored copy. ppsspp bundles
nothing (no Homebrew deps) and duckstation's local-only `package.sh`
deliberately uses an rpath-into-app-Frameworks strategy instead; the
zip/staging steps around the bundling remain per-fork (Sys trees,
metallibs, resources are core-specific).

## Install layout (per core)

    {root}/emulators/libretro/cores/
      <core>_libretro.dylib             — the core (naming: no lib prefix)
      <core>_libretro.dylib.version     — per-core version sidecar (packet 6)
      <core>_libretro_resources/        — optional data payload (dolphin Sys/, ppsspp assets)
      <core>_libretro_libs/             — bundled non-system dylibs

## Release zip (`<core>_libretro.dylib.zip`)

Contains the dylib + `_resources/` + `_libs/` at the zip root. Release body
embeds the upstream merge-base short SHA.

## Dependency bundling (CI)

1. `dylibbundler -od -b -x <dylib> -d <core>_libretro_libs/ -p @loader_path/<core>_libretro_libs/`
2. **Flatten sibling refs**: for every lib inside `_libs/`, rewrite
   inter-lib references to `@loader_path/$(basename)` (`install_name_tool
   -change`) — dylibbundler's single prefix doubles paths for siblings
   (2026-07-04 bug, fixed at source in pcsx2 + dolphin CI).
3. Ad-hoc sign every lib and the core: `codesign --force --sign -`.
4. Assert zero `/usr/local` / `/opt/homebrew` references remain (`otool -L`).

## Arch policy

Declared per core in `manifests/<core>.json` `core_arch`
(`universal|x86_64|arm64`); `scripts/verify-universal.sh` checks every
manifest core. Current truth: duckstation/ppsspp/mgba universal; pcsx2 and
dolphin releases x86_64-only (Rosetta CI).

## Install-side (RetroNest emulator_installer.cpp)

Unzips into `cores/`, dequarantines + ad-hoc signs the core AND its
`_libs/`/`_resources/`, repairs doubled `@loader_path` refs (self-heal for
pre-fix releases), writes the per-core `.version` sidecar.
