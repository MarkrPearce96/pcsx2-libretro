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

#include <string>
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
        // 16 knobs mirroring standalone PCSX2 Graphics/Display sub-tab
        // (Renderer is in Phase 0 under category=Recommended). Defaults
        // come straight from pcsx2_adapter.cpp Graphics/Display rows so
        // a missing options.json reproduces standalone's out-of-the-box
        // behavior.

        // Enum combos — stored values match the INI string verbatim.
        std::string aspect_ratio          = "4:3";
        std::string fmv_aspect_ratio      = "Off";
        int         deinterlace_mode      = 0;
        int         linear_present_mode   = 1;   // Bilinear (Smooth)

        // Int sliders standalone-side; libretro variant exposes enumerated
        // Combo stops (see CoreOptionsGraphics.cpp Append blocks for the
        // stop choices). Standalone's 1%-step slider isn't expressible in
        // libretro's core-options v2 API.
        int  stretch_y    = 100;
        int  crop_left    = 0;
        int  crop_top     = 0;
        int  crop_right   = 0;
        int  crop_bottom  = 0;

        // Patches live under [EmuCore] INI section (not [EmuCore/GS]) —
        // ApplyDefaults handles the section split.
        bool enable_widescreen_patches      = false;
        bool enable_no_interlacing_patches  = false;

        // GS-side display checkboxes.
        bool pcrtc_antiblur            = true;   // standalone default true
        bool integer_scaling           = false;
        bool pcrtc_offsets             = false;
        bool disable_interlace_offset  = false;
        bool pcrtc_overscan            = false;
    } display;

    struct Rendering {
        // 7 knobs mirroring standalone PCSX2 Graphics/Rendering sub-tab.
        // All stored as INI under [EmuCore/GS]. Defaults match
        // pcsx2_adapter.cpp Graphics/Rendering rows verbatim so a missing
        // options.json reproduces standalone's out-of-the-box behavior.
        int  upscale_multiplier      = 1;   // 1x Native
        int  filter                  = 2;   // Bilinear (PS2)
        int  tri_filter              = -1;  // Auto
        int  max_anisotropy          = 0;   // Off
        int  dithering_ps2           = 2;   // Unscaled
        int  accurate_blending_unit  = 1;   // Basic
        bool hw_mipmap               = true;
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
