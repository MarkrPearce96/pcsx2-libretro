// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7c Phase 4: Graphics-category core options.
//
// Owns the kGraphicsDefinitions[] slice of the master core-options
// table. CoreOptions.cpp aggregates this module's slice (plus Emulation
// from Phase 0/1, Audio from Phase 2, and Memory Cards from Phase 3)
// into the single table dispatched via SET_CORE_OPTIONS_V2.
//
// Values is nested by sub-tab (Display, Rendering, TextureReplacement,
// PostProcessing, Osd) so each sub-tab task in Phase 4 adds its slice
// of fields in isolation. The sub-tab names mirror the standalone
// PCSX2 dialog's Graphics widget sub-tabs (the libretro variant
// renders them via the shared GenericSettingsPage sub-tab bar once
// the dialog's hasSubTabs flag is set for Graphics — wiring lands
// in Phase 4 Task 7).

#pragma once

#include "libretro.h"

#include <vector>

namespace Pcsx2Libretro::CoreOptions { struct Resolved; }
class MemorySettingsInterface;

namespace Pcsx2Libretro::CoreOptions::Graphics
{

// Per-sub-tab resolved values. Each nested struct owns the fields its
// sub-tab task populates. Scaffold leaves all five empty; Tasks 2-6
// fill them in turn.
struct Values
{
    struct Display {
        // Phase 4 Task 2 fills these.
    } display;

    struct Rendering {
        // Phase 4 Task 3 fills these.
    } rendering;

    struct TextureReplacement {
        // Phase 4 Task 4 fills these.
    } texture_replacement;

    struct PostProcessing {
        // Phase 4 Task 5 fills these.
    } post_processing;

    struct Osd {
        // Phase 4 Task 6 fills these.
    } osd;
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

} // namespace Pcsx2Libretro::CoreOptions::Graphics
