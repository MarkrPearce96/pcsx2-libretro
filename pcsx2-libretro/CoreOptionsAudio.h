// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7c Phase 2: Audio-category core options.
//
// Owns the kAudioDefinitions[] slice of the master core-options table.
// CoreOptions.cpp aggregates this module's slice (plus Emulation's slice
// from Phase 0/1) into the single table dispatched via SET_CORE_OPTIONS_V2.

#pragma once

#include "libretro.h"

#include <string>
#include <vector>

namespace Pcsx2Libretro::CoreOptions { struct Resolved; }
class MemorySettingsInterface;

namespace Pcsx2Libretro::CoreOptions::Audio
{

// Per-category resolved values. Aggregated by struct Resolved.
//
// Defaults preserve the upstream PCSX2 defaults (SPU2Options ctor +
// AudioStreamParameters defaults) so a missing/empty options.json
// produces identical behavior to today's SP4 baseline.
struct Values
{
    // SPU2/Output/SyncMode — enum string (Pcsx2Config.cpp:1180 s_spu2_sync_mode_names).
    // PCSX2 reads this via SettingsWrapParsedEnum which calls ParseSyncMode;
    // exact-case names "Disabled" / "TimeStretch" are required.
    std::string sync_mode = "TimeStretch";

    // SPU2/Output/BufferMS — ring-buffer size in milliseconds.
    // Default 50 per AudioStreamTypes.h:55 DEFAULT_BUFFER_MS.
    // Consumed by base AudioStream::BaseInitialize → m_buffer_size /
    // m_target_buffer_size (AudioStream.cpp:410-411).
    int buffer_ms = 50;

    // SPU2/Output/StandardVolume — normal-play volume 0..200.
    // spu2.cpp:108 reads as a sample multiplier each retro_run.
    int volume = 100;

    // SPU2/Output/FastForwardVolume — volume during fast-forward 0..200.
    // spu2.cpp:109.
    int ff_volume = 100;

    // SPU2/Output/OutputMuted — global mute. spu2.cpp:110/167.
    bool muted = false;
};

// Append this category's option definitions to the master vector.
// Called once from CoreOptions::BuildDefinitions on first emit.
// Does NOT append the libretro terminator — the master aggregator does that.
void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out);

// Read this category's resolved values from the host. Called from
// CoreOptions::ReadResolved.
void Parse(retro_environment_t cb, Values& out);

// Apply this category's resolved values to the settings interface.
// Called from Pcsx2Libretro::Settings::InitializeDefaults's per-call
// user-options block.
void ApplyDefaults(MemorySettingsInterface& si, const Values& v);

} // namespace Pcsx2Libretro::CoreOptions::Audio
