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
        // 6 knobs mirroring standalone PCSX2 Graphics/Texture Replacement
        // sub-tab. All stored as INI under [EmuCore/GS]. Defaults match
        // pcsx2_adapter.cpp Graphics/Texture-Replacement rows verbatim so
        // a missing options.json reproduces standalone's out-of-the-box
        // behavior. The search-directory picker is dropped (RetroNest
        // manages EmuFolders::Textures from SP1).
        bool load_texture_replacements        = false;
        bool dump_replaceable_textures        = false;
        bool load_texture_replacements_async  = true;  // only non-false default
        bool precache_texture_replacements    = false;
        bool dump_replaceable_mipmaps         = false;
        bool dump_textures_with_fmv_active    = false;
    } texture_replacement;

    struct PostProcessing {
        // 9 knobs mirroring standalone PCSX2 Graphics/Post-Processing
        // sub-tab. All stored as INI under [EmuCore/GS]. Defaults match
        // pcsx2/Config.h:723,741-744,870,894 verbatim — all 5 slider
        // knobs default to 50 (neutral midpoint of the shader formula
        // value/50). Two groups: Sharpening/Anti-Aliasing (CAS×2 + FXAA)
        // and Filters (TVShader, ShadeBoost master + 4 sliders).
        int  cas_mode               = 0;     // GSCASMode::Disabled
        int  cas_sharpness          = 50;
        bool fxaa                   = false;
        int  tv_shader              = 0;
        bool shade_boost            = false;
        int  shade_boost_brightness = 50;
        int  shade_boost_contrast   = 50;
        int  shade_boost_saturation = 50;
        int  shade_boost_gamma      = 50;
    } post_processing;

    struct Osd {
        // 23 knobs mirroring standalone PCSX2 Graphics/On-Screen Display
        // sub-tab. 22 fields stored under [EmuCore/GS];
        // warn_about_unsafe_settings stored under [EmuCore]. Defaults
        // match PCSX2 source verbatim (Config.h:730-733,768-786 +
        // Pcsx2Config.cpp:720-745,1943). OsdBoldText defaults to false
        // because the bitfield zero-inits and there is no explicit
        // assignment in Defaults() — the standalone adapter's
        // default="true" is incorrect (separate SP7c followup).
        int  osd_scale            = 100;   // px-scale, neutral=100
        int  osd_margin           = 10;    // px from screen edge
        int  osd_messages_pos     = 1;     // OsdOverlayPos::TopLeft
        int  osd_performance_pos  = 3;     // OsdOverlayPos::TopRight
        bool osd_bold_text        = false;
        // Performance Stats group (9)
        bool osd_show_speed         = false;
        bool osd_show_fps           = false;
        bool osd_show_vps           = false;
        bool osd_show_resolution    = false;
        bool osd_show_gs_stats      = false;
        bool osd_show_cpu           = false;
        bool osd_show_gpu           = false;
        bool osd_show_indicators    = true;   // ← only non-false in PerfStats
        bool osd_show_frame_times   = false;
        // System Information group (2)
        bool osd_show_hardware_info = false;
        bool osd_show_version       = false;
        // Settings & Inputs group (6)
        bool osd_show_settings              = false;
        bool osd_show_patches               = false;
        bool osd_show_inputs                = false;
        bool osd_show_video_capture         = true;   // libretro-inert
        bool osd_show_input_rec             = true;   // libretro-inert
        bool osd_show_texture_replacements  = false;
        // Messages group (1) — stored under [EmuCore]
        bool warn_about_unsafe_settings     = true;
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
