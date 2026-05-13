// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7b: Libretro core options for pcsx2-libretro.
//
// Declares the option schema emitted via SET_CORE_OPTIONS_V2 at
// retro_set_environment time, and the typed result of reading the
// host's stored user values at retro_load_game time.
//
// Three knobs (smallest valuable cut from the SP7b spec):
//   * pcsx2_renderer  — GS renderer (auto/metal/software/null)
//   * pcsx2_mtvu      — Multi-Threaded VU1
//   * pcsx2_fast_boot — Fast boot (skip PS2 BIOS intro/region screen)
//
// Standalone unit-test gate: define SP7B_TEST_CORE_OPTIONS_ONLY when
// compiling this .cpp directly into tools/test_core_options.cpp to skip
// the FrontendLog dependency on the rest of pcsx2-libretro.

#pragma once

// libretro.h is a single-header public API with no PCSX2 deps — safe to
// include from the header. The test binary picks up the same header via
// the -I flag in the test's compile command.
#include "libretro.h"

namespace Pcsx2Libretro::CoreOptions
{

// Resolved values to feed into Settings.cpp / VMBootParameters.
// Defaults match the SP7a-era hardcoded behavior so an old/missing
// options.json or a host that doesn't support SET_CORE_OPTIONS_V2
// produces identical results to today.
struct Resolved
{
    int  renderer  = -1;    // GSRendererType integer: -1=Auto, 17=Metal, 13=SW, 11=Null
    bool mtvu      = true;  // EmuCore/Speedhacks/vuThread
    bool fast_boot = true;  // EmuCore/EnableFastBoot AND VMBootParameters.fast_boot
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

} // namespace Pcsx2Libretro::CoreOptions
