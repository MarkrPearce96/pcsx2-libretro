// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7c Phase 0 onwards: each category card owns a sibling module
// (CoreOptionsEmulation, CoreOptionsGraphics, ...) that exposes its slice
// of the definitions table plus a Parse helper. This file is now the
// thin aggregator that concatenates them at first-emit time.
//
// Standalone unit-test gate: define CORE_OPTIONS_TEST_ONLY when compiling
// any CoreOptions*.cpp directly into tools/test_core_options.cpp to skip
// the FrontendLog and MemorySettingsInterface dependencies on the rest of
// pcsx2-libretro.

#pragma once

// libretro.h is a single-header public API with no PCSX2 deps — safe to
// include from the header. The test binary picks up the same header via
// the -I flag in the test's compile command.
#include "libretro.h"
#include "CoreOptionsEmulation.h"
#include "CoreOptionsAudio.h"
#include "CoreOptionsMemoryCards.h"

#include <vector>

namespace Pcsx2Libretro::CoreOptions
{

struct Resolved
{
    Pcsx2Libretro::CoreOptions::Emulation::Values    emulation{};
    Pcsx2Libretro::CoreOptions::Audio::Values        audio{};
    Pcsx2Libretro::CoreOptions::MemoryCards::Values  memory_cards{};
    // Future phases append:
    //   Pcsx2Libretro::CoreOptions::Graphics::Values    graphics{};
};

// Emit the option schema to the host. Call once from retro_set_environment
// after stashing the env_cb pointer. Returns false if the host doesn't
// support SET_CORE_OPTIONS_V2 (logged once, not fatal — defaults still apply).
bool EmitCoreOptionsV2(retro_environment_t cb);

// Query the host for current user values. Call once at the top of
// retro_load_game (after BIOS resolution, before Settings::InitializeDefaults).
// NULL returns / unknown enum strings fall back to Resolved's defaults
// with a WARN logged. The fact of reading + the resolved triple are
// logged at INFO once per call.
Resolved ReadResolved(retro_environment_t cb);

// Build (or return the cached) master option-definitions vector.
// First call concatenates each category's AppendDefinitions output and
// appends the libretro terminator. Subsequent calls return the same
// vector by reference — the pointer-to-storage stays valid for the
// process lifetime (function-local static).
//
// Exposed for test_core_options.cpp's structural assertions. Production
// code reaches it indirectly through EmitCoreOptionsV2.
const std::vector<retro_core_option_v2_definition>& BuildDefinitions();

} // namespace Pcsx2Libretro::CoreOptions
