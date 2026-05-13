// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7c Phase 0: Emulation-category core options.
//
// Owns the kEmulationDefinitions[] slice of the master core-options table.
// CoreOptions.cpp aggregates this module's slice (plus future siblings)
// into the single table dispatched via SET_CORE_OPTIONS_V2.

#pragma once

#include "libretro.h"

#include <vector>

namespace Pcsx2Libretro::CoreOptions { struct Resolved; }
class MemorySettingsInterface;

namespace Pcsx2Libretro::CoreOptions::Emulation
{

// Per-category resolved values. Aggregated by struct Resolved.
//
// Fields preserve the SP7a-era defaults so a missing/empty options.json
// produces identical behavior to today.
struct Values
{
    int  renderer  = -1;    // GSRendererType: -1=Auto, 17=Metal, 13=SW, 11=Null
    bool mtvu      = true;  // EmuCore/Speedhacks/vuThread
    bool fast_boot = true;  // EmuCore/EnableFastBoot AND VMBootParameters.fast_boot

    // --- SP7c Phase 1: Speed Control (3 knobs) ---
    // Framerate scalars. INI writes as float via SetFloatValue;
    // PCSX2's ToChars produces shortest-form strings ("1", "0.5", "2", etc.).
    // Value 0.0 maps to "Unlimited".
    float normal_speed       = 1.0f;  // Framerate/NominalScalar
    float fast_forward_speed = 2.0f;  // Framerate/TurboScalar
    float slow_motion_speed  = 0.5f;  // Framerate/SlomoScalar
};

// Append this category's option definitions to the master vector.
// Called once from CoreOptions::BuildDefinitions on first emit.
// Does NOT append the libretro terminator — that is the master
// aggregator's responsibility.
void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out);

// Read this category's resolved values from the host. Called from
// CoreOptions::ReadResolved.
void Parse(retro_environment_t cb, Values& out);

// Apply this category's resolved values to the settings interface.
// Called from Pcsx2Libretro::Settings::InitializeDefaults.
void ApplyDefaults(MemorySettingsInterface& si, const Values& v);

} // namespace Pcsx2Libretro::CoreOptions::Emulation
