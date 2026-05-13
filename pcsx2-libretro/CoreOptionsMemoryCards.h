// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7c Phase 3: Memory Cards-category core options.
//
// Owns the kMemoryCardsDefinitions[] slice of the master core-options
// table. CoreOptions.cpp aggregates this module's slice (plus Emulation's
// from Phase 0/1 and Audio's from Phase 2) into the single table dispatched
// via SET_CORE_OPTIONS_V2.

#pragma once

#include "libretro.h"

#include <vector>

namespace Pcsx2Libretro::CoreOptions { struct Resolved; }
class MemorySettingsInterface;

namespace Pcsx2Libretro::CoreOptions::MemoryCards
{

// Per-category resolved values. Aggregated by struct Resolved.
//
// Defaults match the upstream PCSX2 defaults shown by the standalone
// PCSX2 dialog (cpp/src/adapters/pcsx2_adapter.cpp:987-1028) so a
// missing/empty options.json reproduces standalone's out-of-the-box
// behavior. Slot2_Enable defaults to true here (matching standalone),
// not false — Phase 3 deliberately switches the pre-SP7c default for
// parity. See plan section "Decision: Slot2_Enable default".
struct Values
{
    // MemoryCards/Slot1_Enable — inserts a virtual memcard into Slot 1.
    // Pcsx2Config.cpp:2051 reads via SettingsWrapEntry as `Mcd[0].Enabled`.
    bool slot1_enable = true;

    // MemoryCards/Slot2_Enable — inserts a virtual memcard into Slot 2.
    // Pcsx2Config.cpp:2051 reads via SettingsWrapEntry as `Mcd[1].Enabled`.
    bool slot2_enable = true;

    // MemoryCards/Multitap1_Slot{2,3,4}_Enable — enables additional
    // memcard slots when Multitap 1 is connected. Pcsx2Config.cpp:2062
    // reads via SettingsWrapEntry. Independent of any Multitap1
    // master toggle (which lives in the unrelated Pcsx2 section).
    bool multitap1_slot2 = false;
    bool multitap1_slot3 = false;
    bool multitap1_slot4 = false;
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

} // namespace Pcsx2Libretro::CoreOptions::MemoryCards
