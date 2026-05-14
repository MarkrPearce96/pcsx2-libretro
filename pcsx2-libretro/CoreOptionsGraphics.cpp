// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsGraphics.h"

#ifdef CORE_OPTIONS_TEST_ONLY
#include <cstdarg>
#include <cstdio>
static void FrontendLog(int /*level*/, const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    std::fputc('\n', stderr);
    va_end(ap);
}
#else
#include "LibretroFrontend.h"                  // FrontendLog
#include "common/MemorySettingsInterface.h"    // MemorySettingsInterface
#endif

#include <cstdlib>
#include <cstring>

namespace Pcsx2Libretro::CoreOptions::Graphics
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)
{
    // ── Display sub-tab (16 knobs) — Phase 4 Task 2 ────────────────────
    //
    // Renderer is the 17th standalone Display row but already lives under
    // category=Recommended from Phase 0; Phase 5 will cross-list it under
    // Graphics/Display.
    //
    // 5 int-slider knobs (StretchY, Crop{Left,Top,Right,Bottom}) become
    // Combo with enumerated stops because libretro core options v2 is
    // Combo-only. Stops chosen so standalone's default + extremes are
    // reachable and steps cluster near the default where users actually
    // tweak: StretchY 50/75/90/100/110/125/150/200/300%; Crop* 0/1/2/3/5/
    // 10/15/20/30/50/100 px (default 0 for all four).
    //
    // EnableWideScreenPatches + EnableNoInterlacingPatches store under
    // [EmuCore] (not [EmuCore/GS]) — ApplyDefaults handles the split.
    //
    // Each entry is a literal out.push_back({...}) block (no lambda
    // helper) so tools/check_schema_fidelity.py's CORE_BLOCK_RE
    // recognizes them.

    out.push_back({
        "pcsx2_aspect_ratio",
        "Aspect Ratio",
        nullptr,
        "Controls the aspect ratio of the emulated display. Auto picks "
        "4:3 for interlaced games and 3:2 for progressive games. 16:9 "
        "stretches the image for widescreen TVs; Stretch fills the whole "
        "window.",
        nullptr,
        nullptr,
        {
            { "Auto 4:3/3:2", "Auto (4:3 Interlaced / 3:2 Progressive)" },
            { "4:3",          "4:3 (Standard)" },
            { "16:9",         "16:9 (Widescreen)" },
            { "10:7",         "10:7 (Full/Native)" },
            { "Stretch",      "Stretch" },
            { nullptr,        nullptr },
        },
        "4:3",
    });

    out.push_back({
        "pcsx2_fmv_aspect_ratio",
        "FMV Aspect Ratio Override",
        nullptr,
        "Overrides the aspect ratio only while full-motion video (FMV) "
        "is playing. Useful for games with widescreen cutscenes inside "
        "a 4:3 main game.",
        nullptr,
        nullptr,
        {
            { "Off",          "Off (Default)" },
            { "Auto 4:3/3:2", "Auto (4:3 Interlaced / 3:2 Progressive)" },
            { "4:3",          "4:3 (Standard)" },
            { "16:9",         "16:9 (Widescreen)" },
            { "10:7",         "10:7 (Full/Native)" },
            { nullptr,        nullptr },
        },
        "Off",
    });

    out.push_back({
        "pcsx2_deinterlace_mode",
        "Deinterlacing",
        nullptr,
        "Selects how interlaced frames are combined for progressive "
        "display. Automatic picks the best option per game; Weave "
        "preserves detail; Bob and Blend smooth motion at the cost of "
        "vertical resolution.",
        nullptr,
        nullptr,
        {
            { "0", "Automatic (Default)" },
            { "1", "Off" },
            { "2", "Weave (Top)" },
            { "3", "Weave (Bottom)" },
            { "4", "Bob (Top)" },
            { "5", "Bob (Bottom)" },
            { "6", "Blend (Top)" },
            { "7", "Blend (Bottom)" },
            { "8", "Adaptive (Top)" },
            { "9", "Adaptive (Bottom)" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_linear_present_mode",
        "Bilinear Filtering",
        nullptr,
        "Applies a bilinear filter when scaling the final image to the "
        "window. Smooth is the standard option; Sharp uses a pixel-art-"
        "friendly variant that keeps edges crisp.",
        nullptr,
        nullptr,
        {
            { "0", "None" },
            { "1", "Bilinear (Smooth) (Default)" },
            { "2", "Bilinear (Sharp)" },
            { nullptr, nullptr },
        },
        "1",
    });

    out.push_back({
        "pcsx2_stretch_y",
        "Vertical Stretch",
        nullptr,
        "Multiplies the display height after aspect-ratio fitting. "
        "Values above 100% make the image taller than its letterbox; "
        "values below leave extra vertical space. Standalone PCSX2 "
        "exposes a 10-300% slider; libretro offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "50",  "50%" },
            { "75",  "75%" },
            { "90",  "90%" },
            { "100", "100% (Default)" },
            { "110", "110%" },
            { "125", "125%" },
            { "150", "150%" },
            { "200", "200%" },
            { "300", "300%" },
            { nullptr, nullptr },
        },
        "100",
    });

    // Crop{Left,Top,Right,Bottom} all share the same stops. Inline per
    // entry (D1 — defer CORE_BLOCK_RE hardening for shared identifier
    // refs). Default 0 for all four.
    out.push_back({
        "pcsx2_crop_left",
        "Crop - Left",
        nullptr,
        "Trims pixels from the left edge of the source image before "
        "it's fit to the display window. Useful for games with garbage "
        "pixels at the border.",
        nullptr,
        nullptr,
        {
            { "0",   "0 px (Default)" },
            { "1",   "1 px" },
            { "2",   "2 px" },
            { "3",   "3 px" },
            { "5",   "5 px" },
            { "10",  "10 px" },
            { "15",  "15 px" },
            { "20",  "20 px" },
            { "30",  "30 px" },
            { "50",  "50 px" },
            { "100", "100 px" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_crop_top",
        "Crop - Top",
        nullptr,
        "Trims pixels from the top edge of the source image before "
        "it's fit to the display window.",
        nullptr,
        nullptr,
        {
            { "0",   "0 px (Default)" },
            { "1",   "1 px" },
            { "2",   "2 px" },
            { "3",   "3 px" },
            { "5",   "5 px" },
            { "10",  "10 px" },
            { "15",  "15 px" },
            { "20",  "20 px" },
            { "30",  "30 px" },
            { "50",  "50 px" },
            { "100", "100 px" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_crop_right",
        "Crop - Right",
        nullptr,
        "Trims pixels from the right edge of the source image before "
        "it's fit to the display window.",
        nullptr,
        nullptr,
        {
            { "0",   "0 px (Default)" },
            { "1",   "1 px" },
            { "2",   "2 px" },
            { "3",   "3 px" },
            { "5",   "5 px" },
            { "10",  "10 px" },
            { "15",  "15 px" },
            { "20",  "20 px" },
            { "30",  "30 px" },
            { "50",  "50 px" },
            { "100", "100 px" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_crop_bottom",
        "Crop - Bottom",
        nullptr,
        "Trims pixels from the bottom edge of the source image before "
        "it's fit to the display window.",
        nullptr,
        nullptr,
        {
            { "0",   "0 px (Default)" },
            { "1",   "1 px" },
            { "2",   "2 px" },
            { "3",   "3 px" },
            { "5",   "5 px" },
            { "10",  "10 px" },
            { "15",  "15 px" },
            { "20",  "20 px" },
            { "30",  "30 px" },
            { "50",  "50 px" },
            { "100", "100 px" },
            { nullptr, nullptr },
        },
        "0",
    });

    // ── Display bools (7) — all standard {enabled,disabled} pairs ──
    out.push_back({
        "pcsx2_enable_widescreen_patches",
        "Apply Widescreen Patches",
        nullptr,
        "Automatically applies community widescreen patches to supported "
        "games. Reshapes the rendering to true 16:9 instead of stretching "
        "the 4:3 picture.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_enable_no_interlacing_patches",
        "Apply No-Interlacing Patches",
        nullptr,
        "Automatically applies community no-interlacing patches to "
        "supported games. Removes flicker in games that render in "
        "interlaced mode.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_pcrtc_antiblur",
        "Anti-Blur",
        nullptr,
        "Enables internal anti-blur hacks that remove the PS2's GS smear "
        "on commonly-affected games. Safe to leave on.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_integer_scaling",
        "Integer Scaling",
        nullptr,
        "Snaps the rendered image to an integer multiple of the source "
        "pixel size. Produces crisp pixel-art scaling at the cost of "
        "leaving letterbox bars.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_pcrtc_offsets",
        "Screen Offsets",
        nullptr,
        "Enables PCRTC offsets so the screen is positioned exactly where "
        "the game requests. Fixes games that deliberately offset the "
        "viewport.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_disable_interlace_offset",
        "Disable Interlace Offset",
        nullptr,
        "Disables the half-pixel interlace offset which can reduce "
        "jitter on some games that render at half vertical resolution.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_pcrtc_overscan",
        "Show Overscan",
        nullptr,
        "Shows the overscan area of the display that would normally be "
        "hidden by a CRT bezel. Exposes any garbage the game draws "
        "outside the safe area.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    // ── Rendering sub-tab (7 knobs) — Phase 4 Task 3 ───────────────────
    //
    // Standalone exposes upscale_multiplier as a 12-step slider (1x–12x)
    // and MaxAnisotropy as a 0–16 slider; the libretro variant mirrors
    // standalone's enumerated stops verbatim (pcsx2_adapter.cpp:451-503).
    // Value strings are the INI ints as strings — Apply maps via
    // SetIntValue. filter's dependsOn (TriFilter!=2 && TriFilter!=3) is
    // expressed on the host side as the libretro-key form
    // (pcsx2_tri_filter!=2 && pcsx2_tri_filter!=3).
    out.push_back({
        "pcsx2_upscale_multiplier",
        "Internal Resolution",
        nullptr,
        "Sets the internal rendering resolution. Higher values produce "
        "sharper visuals at the cost of GPU performance.",
        nullptr,
        nullptr,
        {
            { "1",  "1x Native (PS2) (Default)" },
            { "2",  "2x Native (~720px/HD)" },
            { "3",  "3x Native (~1080px/FHD)" },
            { "4",  "4x Native (~1440px/QHD)" },
            { "5",  "5x Native (~1800px/QHD+)" },
            { "6",  "6x Native (~2160px/4K UHD)" },
            { "7",  "7x Native (~2520px)" },
            { "8",  "8x Native (~2880px/5K UHD)" },
            { "9",  "9x Native (~3240px)" },
            { "10", "10x Native (~3600px/6K UHD)" },
            { "11", "11x Native (~3960px)" },
            { "12", "12x Native (~4320px/8K UHD)" },
            { nullptr, nullptr },
        },
        "1",
    });

    out.push_back({
        "pcsx2_filter",
        "Texture Filtering",
        nullptr,
        "Controls how textures are sampled when rendered. Bilinear (PS2) "
        "matches the original hardware behavior; Forced options ignore "
        "the game's preference.",
        nullptr,
        nullptr,
        {
            { "0", "Nearest" },
            { "1", "Bilinear (Forced)" },
            { "2", "Bilinear (PS2) (Default)" },
            { "3", "Bilinear (Forced excluding sprite)" },
            { nullptr, nullptr },
        },
        "2",
    });

    out.push_back({
        "pcsx2_tri_filter",
        "Trilinear Filtering",
        nullptr,
        "Enables trilinear filtering for smoother transitions between "
        "mipmap levels. Auto leaves this decision to each game.",
        nullptr,
        nullptr,
        {
            { "-1", "Auto (Default)" },
            { "0",  "Off" },
            { "1",  "Trilinear (PS2)" },
            { "2",  "Trilinear (Forced)" },
            { nullptr, nullptr },
        },
        "-1",
    });

    out.push_back({
        "pcsx2_max_anisotropy",
        "Anisotropic Filtering",
        nullptr,
        "Improves texture clarity at oblique viewing angles. Low cost on "
        "modern GPUs and generally safe to raise.",
        nullptr,
        nullptr,
        {
            { "0",  "Off (Default)" },
            { "2",  "2x" },
            { "4",  "4x" },
            { "8",  "8x" },
            { "16", "16x" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_dithering_ps2",
        "Dithering",
        nullptr,
        "Controls how PS2 dithering patterns are applied to upscaled "
        "rendering. Unscaled matches the original appearance.",
        nullptr,
        nullptr,
        {
            { "0", "Off" },
            { "1", "Scaled" },
            { "2", "Unscaled (Default)" },
            { "3", "Force 32bit" },
            { nullptr, nullptr },
        },
        "2",
    });

    out.push_back({
        "pcsx2_accurate_blending_unit",
        "Blending Accuracy",
        nullptr,
        "Controls how accurately PS2 blending operations are emulated. "
        "Higher levels improve compatibility with heavy effects at a "
        "performance cost.",
        nullptr,
        nullptr,
        {
            { "0", "Minimum" },
            { "1", "Basic (Default)" },
            { "2", "Medium" },
            { "3", "High" },
            { "4", "Full" },
            { "5", "Maximum" },
            { nullptr, nullptr },
        },
        "1",
    });

    out.push_back({
        "pcsx2_hw_mipmap",
        "Hardware Mipmapping",
        nullptr,
        "Emulates PS2 mipmapping when the hardware renderer is active. "
        "Fixes texture aliasing at distance in supported games.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    // ── Texture Replacement sub-tab (6 knobs) — Phase 4 Task 4 ─────────
    //
    // All bools under [EmuCore/GS]. Standalone gates every row on
    // Renderer!=13 (Software). The libretro variant translates that to
    // pcsx2_renderer!=software on the host side (dependsOn string),
    // since pcsx2_renderer is a string-valued Combo, not the int enum.
    // The search-directory picker is dropped (RetroNest manages
    // EmuFolders::Textures from SP1). Per-game scoping is native:
    // GSTextureReplacements::Path::Combine(EmuFolders::Textures,
    // s_current_serial) — no extra wiring needed.
    out.push_back({
        "pcsx2_load_texture_replacements",
        "Load Textures",
        nullptr,
        "Loads replacement textures from the texture-replacement folder "
        "(per-game subdirectory keyed by disc serial). Only effective "
        "with a hardware renderer.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_dump_replaceable_textures",
        "Dump Textures",
        nullptr,
        "Dumps the game's textures to disk so they can be edited and "
        "loaded back as replacements. Writes to the per-game texture-"
        "replacement subdirectory. Only effective with a hardware "
        "renderer.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_load_texture_replacements_async",
        "Asynchronous Texture Loading",
        nullptr,
        "Loads replacement textures on a background thread to avoid "
        "stutter at first sight of each texture. Disable only for "
        "deterministic capture or debugging.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_precache_texture_replacements",
        "Precache Replacements",
        nullptr,
        "Loads every replacement texture for the current game at boot "
        "instead of on-demand. Uses more memory but eliminates load "
        "stutter mid-game.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_dump_replaceable_mipmaps",
        "Dump Mipmaps",
        nullptr,
        "When dumping textures, also writes each mipmap level. Useful "
        "for replacing distance LODs explicitly. Only meaningful with "
        "Dump Textures enabled.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_dump_textures_with_fmv_active",
        "Dump Textures During FMV",
        nullptr,
        "Includes textures used during full-motion video in dumps. Off "
        "by default because FMV frames produce a large volume of one-"
        "off textures.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    // ── Post-Processing sub-tab (9 knobs) — Phase 4 Task 5 ─────────────
    //
    // Two groups: Sharpening/Anti-Aliasing (CAS Mode + CAS Sharpness +
    // FXAA) and Filters (TV Shader + ShadeBoost master + 4 ShadeBoost
    // sliders). All stored under [EmuCore/GS].
    //
    // 5 standalone-side int sliders (CAS Sharpness + 4 ShadeBoost rows)
    // become Combo with stops 0/25/50/75/100 because libretro core
    // options v2 is Combo-only. The shared 5-stop value list is inlined
    // at each call site (matches Task 2's Crop{Left,Top,Right,Bottom}
    // pattern — keeps grep-ability, low duplication cost). Default 50
    // for all 5 hits a stop and is the neutral midpoint of PCSX2's
    // shader formula value/50.
    //
    // dependsOn for Sharpness + ShadeBoost slaves is expressed on the
    // host side (Combo dependsOn strings). Within-Graphics gates resolve
    // correctly via GenericSettingsPage::refreshDependencies (cross-
    // category limitation does not apply — see SP7c memory
    // cross_category_dependson_limitation).

    out.push_back({
        "pcsx2_cas_mode",
        "Contrast Adaptive Sharpening (CAS)",
        nullptr,
        "AMD's Contrast Adaptive Sharpening pass on the final image. "
        "Sharpen Only sharpens at the internal render resolution; "
        "Sharpen and Resize sharpens at the display resolution.",
        nullptr,
        nullptr,
        {
            { "0", "Disabled (Default)" },
            { "1", "Sharpen Only (Internal Resolution)" },
            { "2", "Sharpen and Resize (Display Resolution)" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_cas_sharpness",
        "CAS Sharpness",
        nullptr,
        "Strength of the CAS sharpening pass. Higher values produce a "
        "sharper image with more visible noise. Standalone PCSX2 exposes "
        "a 1–100% slider; libretro offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0%" },
            { "25",  "25%" },
            { "50",  "50% (Default)" },
            { "75",  "75%" },
            { "100", "100%" },
            { nullptr, nullptr },
        },
        "50",
    });

    out.push_back({
        "pcsx2_fxaa",
        "FXAA",
        nullptr,
        "Fast Approximate Anti-Aliasing. A single-pass shader that "
        "softens jagged edges with low GPU cost.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_tv_shader",
        "TV Shader",
        nullptr,
        "Applies a CRT-style filter to the final output for an authentic "
        "retro look. None disables the filter.",
        nullptr,
        nullptr,
        {
            { "0", "None (Default)" },
            { "1", "Scanline Filter" },
            { "2", "Diagonal Filter" },
            { "3", "Triangular Filter" },
            { "4", "Wave Filter" },
            { "5", "Lottes CRT" },
            { "6", "4xRGSS Downsampling" },
            { "7", "NxAGSS Downsampling" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_shade_boost",
        "Shade Boost",
        nullptr,
        "Master toggle for manual brightness, contrast, saturation, and "
        "gamma adjustment via the Shade Boost shader.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_shade_boost_brightness",
        "Shade Boost — Brightness",
        nullptr,
        "Brightness multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); 0% blacks out the image; 100% is double "
        "brightness. Standalone PCSX2 exposes a 1–100% slider; libretro "
        "offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0%" },
            { "25",  "25%" },
            { "50",  "50% (Default — Neutral)" },
            { "75",  "75%" },
            { "100", "100%" },
            { nullptr, nullptr },
        },
        "50",
    });

    out.push_back({
        "pcsx2_shade_boost_contrast",
        "Shade Boost — Contrast",
        nullptr,
        "Contrast multiplier when Shade Boost is enabled. 50% is neutral "
        "(no change). Standalone PCSX2 exposes a 1–100% slider; libretro "
        "offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0%" },
            { "25",  "25%" },
            { "50",  "50% (Default — Neutral)" },
            { "75",  "75%" },
            { "100", "100%" },
            { nullptr, nullptr },
        },
        "50",
    });

    out.push_back({
        "pcsx2_shade_boost_saturation",
        "Shade Boost — Saturation",
        nullptr,
        "Color-saturation multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); 0% produces grayscale. Standalone PCSX2 "
        "exposes a 1–100% slider; libretro offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0%" },
            { "25",  "25%" },
            { "50",  "50% (Default — Neutral)" },
            { "75",  "75%" },
            { "100", "100%" },
            { nullptr, nullptr },
        },
        "50",
    });

    out.push_back({
        "pcsx2_shade_boost_gamma",
        "Shade Boost — Gamma",
        nullptr,
        "Gamma-correction multiplier when Shade Boost is enabled. 50% is "
        "neutral (no change); lower values darken midtones, higher "
        "values brighten midtones. Standalone PCSX2 exposes a 1–100% "
        "slider; libretro offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0%" },
            { "25",  "25%" },
            { "50",  "50% (Default — Neutral)" },
            { "75",  "75%" },
            { "100", "100%" },
            { nullptr, nullptr },
        },
        "50",
    });

    // ── On-Screen Display sub-tab (Phase 4 Task 6) ──
    //
    // 23 knobs. 22 stored under [EmuCore/GS]; warn_about_unsafe_settings
    // under [EmuCore] (split-section like Task 2 patches rows). Defaults
    // verified against pcsx2/Config.h:730-733,768-786 + Pcsx2Config.cpp:
    // 720-745,1943.
    //
    // OsdScale + OsdMargin are int sliders standalone-side; libretro v2
    // is Combo-only, so they use enumerated stops (Task 2 Crop + Task 5
    // CAS Sharpness precedent). 8 stops each, default sits on a stop.
    //
    // OsdMessagesPos + OsdPerformancePos use the 10-stop OsdOverlayPos
    // enum (None=0, TopLeft=1, TopCenter=2, TopRight=3, CenterLeft=4,
    // Center=5, CenterRight=6, BottomLeft=7, BottomCenter=8,
    // BottomRight=9). Per Config.h:341-353.

    // -- "On-Screen Display" group (5 rows) --

    out.push_back({
        "pcsx2_osd_scale",
        "OSD Scale",
        nullptr,
        "Global multiplier applied to every OSD overlay. 100% matches "
        "standalone PCSX2's default size. Standalone exposes a "
        "25-500% slider; libretro offers enumerated stops.",
        nullptr,
        nullptr,
        {
            { "50",  "50%" },
            { "75",  "75%" },
            { "100", "100% (Default)" },
            { "125", "125%" },
            { "150", "150%" },
            { "200", "200%" },
            { "300", "300%" },
            { "500", "500%" },
            { nullptr, nullptr },
        },
        "100",
    });

    out.push_back({
        "pcsx2_osd_margin",
        "OSD Margin",
        nullptr,
        "Pixel offset between the OSD elements and the screen edge. "
        "Standalone exposes a 0-100px slider; libretro offers "
        "enumerated stops.",
        nullptr,
        nullptr,
        {
            { "0",   "0px" },
            { "5",   "5px" },
            { "10",  "10px (Default)" },
            { "15",  "15px" },
            { "20",  "20px" },
            { "30",  "30px" },
            { "50",  "50px" },
            { "100", "100px" },
            { nullptr, nullptr },
        },
        "10",
    });

    out.push_back({
        "pcsx2_osd_messages_pos",
        "OSD Messages Position",
        nullptr,
        "Corner where transient messages (save-state loaded, shader "
        "reload, etc.) are drawn. Set to None to hide them entirely.",
        nullptr,
        nullptr,
        {
            { "0", "None" },
            { "1", "Top Left (Default)" },
            { "2", "Top Center" },
            { "3", "Top Right" },
            { "4", "Center Left" },
            { "5", "Center" },
            { "6", "Center Right" },
            { "7", "Bottom Left" },
            { "8", "Bottom Center" },
            { "9", "Bottom Right" },
            { nullptr, nullptr },
        },
        "1",
    });

    out.push_back({
        "pcsx2_osd_performance_pos",
        "OSD Performance Position",
        nullptr,
        "Corner where the performance stats column (FPS/Speed/CPU/GPU/"
        "etc.) is drawn. Set to None to hide the column and grey out "
        "every Performance Stats / System Information toggle.",
        nullptr,
        nullptr,
        {
            { "0", "None" },
            { "1", "Top Left" },
            { "2", "Top Center" },
            { "3", "Top Right (Default)" },
            { "4", "Center Left" },
            { "5", "Center" },
            { "6", "Center Right" },
            { "7", "Bottom Left" },
            { "8", "Bottom Center" },
            { "9", "Bottom Right" },
            { nullptr, nullptr },
        },
        "3",
    });

    out.push_back({
        "pcsx2_osd_bold_text",
        "OSD Text Style (Bold)",
        nullptr,
        "Renders OSD text in bold. Easier to read on bright scenes.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    // -- "Performance Stats" group (9 rows; host-side dependsOn pcsx2_osd_performance_pos!=0) --

    out.push_back({
        "pcsx2_osd_show_speed",
        "Show Speed Percentages",
        nullptr,
        "Displays the emulation speed as a percentage. Red below 95%, "
        "green above 105%.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_fps",
        "Show FPS",
        nullptr,
        "Displays the current frame rate reported by the GS. Useful "
        "for spotting performance issues.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_vps",
        "Show VPS",
        nullptr,
        "Displays vertical syncs per second — the PS2 display refresh "
        "reported by the GS.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_resolution",
        "Show Resolution",
        nullptr,
        "Displays the PS2 internal render resolution and interlacing "
        "mode.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_gs_stats",
        "Show GS Statistics",
        nullptr,
        "Displays per-frame GS statistics: draw-call count, VRAM use, "
        "and a frame-time summary.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_cpu",
        "Show CPU Usage",
        nullptr,
        "Displays per-component CPU usage (EE, GS, VU).",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_gpu",
        "Show GPU Usage",
        nullptr,
        "Displays GPU usage percentage and frame time in milliseconds.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_indicators",
        "Show Status Indicators",
        nullptr,
        "Displays icons for pause, fast-forward, slow-motion, and "
        "turbo modes in the top-right corner.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled (Default)" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_osd_show_frame_times",
        "Show Frame Times",
        nullptr,
        "Displays a rolling graph of recent frame times to visualise "
        "stutter.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    // -- "System Information" group (2 rows; host-side dependsOn pcsx2_osd_performance_pos!=0) --

    out.push_back({
        "pcsx2_osd_show_hardware_info",
        "Show Hardware Info",
        nullptr,
        "Displays the CPU and GPU model names as two lines in the "
        "performance column.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_version",
        "Show PCSX2 Version",
        nullptr,
        "Displays the PCSX2 version string in the performance column.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    // -- "Settings & Inputs" group (6 rows; mixed dependsOn host-side) --

    out.push_back({
        "pcsx2_osd_show_settings",
        "Show Settings",
        nullptr,
        "Displays a compact summary of active emulation settings in "
        "the bottom-right corner.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_patches",
        "Show Patches",
        nullptr,
        "Appends active patches (widescreen, no-interlacing, etc.) to "
        "the settings line. Requires Show Settings to be enabled.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_inputs",
        "Show Inputs",
        nullptr,
        "Displays the current controller input state at the "
        "bottom-left corner.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_osd_show_video_capture",
        "Show Video Capture Status",
        nullptr,
        "Displays a recording indicator while video capture is "
        "active. (Inert in the libretro variant — no FFmpeg capture "
        "is driven from this build.)",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled (Default)" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_osd_show_input_rec",
        "Show Input Recording Status",
        nullptr,
        "Displays an indicator while input recording is active. "
        "(Inert in the libretro variant — input recording UI is not "
        "driven from this build.)",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled (Default)" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_osd_show_texture_replacements",
        "Show Texture Replacement Status",
        nullptr,
        "Displays an indicator when replacement textures are loaded "
        "for the current game. Requires Texture Replacement → Load "
        "Textures to be enabled.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled (Default)" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    // -- "Messages" group (1 row; host-side dependsOn pcsx2_osd_messages_pos!=0) --
    // NOTE: stored under [EmuCore] (not [EmuCore/GS]). Same split as
    // Task 2's widescreen/no-interlacing patches rows.

    out.push_back({
        "pcsx2_warn_about_unsafe_settings",
        "Warn About Unsafe Settings",
        nullptr,
        "Shows a startup warning if any unsafe settings are enabled.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled (Default)" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });
}

void Parse(retro_environment_t cb, Values& out)
{
    if (!cb) return;

    auto query = [&cb](const char* key) -> const char* {
        retro_variable var{};
        var.key = key;
        if (cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
            return var.value;
        return nullptr;
    };

    auto parse_bool = [](const char* s) -> bool {
        return s && std::strcmp(s, "enabled") == 0;
    };

    auto parse_int = [](const char* s, int fallback) -> int {
        if (!s) return fallback;
        char* end = nullptr;
        const long n = std::strtol(s, &end, 10);
        if (end == s) return fallback;
        return static_cast<int>(n);
    };

    // ── Display sub-tab ──
    if (const char* v = query("pcsx2_aspect_ratio"))
        out.display.aspect_ratio = v;
    if (const char* v = query("pcsx2_fmv_aspect_ratio"))
        out.display.fmv_aspect_ratio = v;
    if (const char* v = query("pcsx2_deinterlace_mode"))
        out.display.deinterlace_mode = parse_int(v, 0);
    if (const char* v = query("pcsx2_linear_present_mode"))
        out.display.linear_present_mode = parse_int(v, 1);

    if (const char* v = query("pcsx2_stretch_y"))
        out.display.stretch_y = parse_int(v, 100);
    if (const char* v = query("pcsx2_crop_left"))
        out.display.crop_left = parse_int(v, 0);
    if (const char* v = query("pcsx2_crop_top"))
        out.display.crop_top = parse_int(v, 0);
    if (const char* v = query("pcsx2_crop_right"))
        out.display.crop_right = parse_int(v, 0);
    if (const char* v = query("pcsx2_crop_bottom"))
        out.display.crop_bottom = parse_int(v, 0);

    if (const char* v = query("pcsx2_enable_widescreen_patches"))
        out.display.enable_widescreen_patches = parse_bool(v);
    if (const char* v = query("pcsx2_enable_no_interlacing_patches"))
        out.display.enable_no_interlacing_patches = parse_bool(v);
    if (const char* v = query("pcsx2_pcrtc_antiblur"))
        out.display.pcrtc_antiblur = parse_bool(v);
    if (const char* v = query("pcsx2_integer_scaling"))
        out.display.integer_scaling = parse_bool(v);
    if (const char* v = query("pcsx2_pcrtc_offsets"))
        out.display.pcrtc_offsets = parse_bool(v);
    if (const char* v = query("pcsx2_disable_interlace_offset"))
        out.display.disable_interlace_offset = parse_bool(v);
    if (const char* v = query("pcsx2_pcrtc_overscan"))
        out.display.pcrtc_overscan = parse_bool(v);

    // ── Rendering sub-tab ──
    if (const char* v = query("pcsx2_upscale_multiplier"))
        out.rendering.upscale_multiplier = parse_int(v, 1);
    if (const char* v = query("pcsx2_filter"))
        out.rendering.filter = parse_int(v, 2);
    if (const char* v = query("pcsx2_tri_filter"))
        out.rendering.tri_filter = parse_int(v, -1);
    if (const char* v = query("pcsx2_max_anisotropy"))
        out.rendering.max_anisotropy = parse_int(v, 0);
    if (const char* v = query("pcsx2_dithering_ps2"))
        out.rendering.dithering_ps2 = parse_int(v, 2);
    if (const char* v = query("pcsx2_accurate_blending_unit"))
        out.rendering.accurate_blending_unit = parse_int(v, 1);
    if (const char* v = query("pcsx2_hw_mipmap"))
        out.rendering.hw_mipmap = parse_bool(v);

    // ── Texture Replacement sub-tab ──
    if (const char* v = query("pcsx2_load_texture_replacements"))
        out.texture_replacement.load_texture_replacements = parse_bool(v);
    if (const char* v = query("pcsx2_dump_replaceable_textures"))
        out.texture_replacement.dump_replaceable_textures = parse_bool(v);
    if (const char* v = query("pcsx2_load_texture_replacements_async"))
        out.texture_replacement.load_texture_replacements_async = parse_bool(v);
    if (const char* v = query("pcsx2_precache_texture_replacements"))
        out.texture_replacement.precache_texture_replacements = parse_bool(v);
    if (const char* v = query("pcsx2_dump_replaceable_mipmaps"))
        out.texture_replacement.dump_replaceable_mipmaps = parse_bool(v);
    if (const char* v = query("pcsx2_dump_textures_with_fmv_active"))
        out.texture_replacement.dump_textures_with_fmv_active = parse_bool(v);

    // ── Post-Processing sub-tab ──
    if (const char* v = query("pcsx2_cas_mode"))
        out.post_processing.cas_mode = parse_int(v, 0);
    if (const char* v = query("pcsx2_cas_sharpness"))
        out.post_processing.cas_sharpness = parse_int(v, 50);
    if (const char* v = query("pcsx2_fxaa"))
        out.post_processing.fxaa = parse_bool(v);
    if (const char* v = query("pcsx2_tv_shader"))
        out.post_processing.tv_shader = parse_int(v, 0);
    if (const char* v = query("pcsx2_shade_boost"))
        out.post_processing.shade_boost = parse_bool(v);
    if (const char* v = query("pcsx2_shade_boost_brightness"))
        out.post_processing.shade_boost_brightness = parse_int(v, 50);
    if (const char* v = query("pcsx2_shade_boost_contrast"))
        out.post_processing.shade_boost_contrast = parse_int(v, 50);
    if (const char* v = query("pcsx2_shade_boost_saturation"))
        out.post_processing.shade_boost_saturation = parse_int(v, 50);
    if (const char* v = query("pcsx2_shade_boost_gamma"))
        out.post_processing.shade_boost_gamma = parse_int(v, 50);

    // ── On-Screen Display sub-tab ──
    if (const char* v = query("pcsx2_osd_scale"))
        out.osd.osd_scale = parse_int(v, 100);
    if (const char* v = query("pcsx2_osd_margin"))
        out.osd.osd_margin = parse_int(v, 10);
    if (const char* v = query("pcsx2_osd_messages_pos"))
        out.osd.osd_messages_pos = parse_int(v, 1);
    if (const char* v = query("pcsx2_osd_performance_pos"))
        out.osd.osd_performance_pos = parse_int(v, 3);
    if (const char* v = query("pcsx2_osd_bold_text"))
        out.osd.osd_bold_text = parse_bool(v);

    if (const char* v = query("pcsx2_osd_show_speed"))
        out.osd.osd_show_speed = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_fps"))
        out.osd.osd_show_fps = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_vps"))
        out.osd.osd_show_vps = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_resolution"))
        out.osd.osd_show_resolution = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_gs_stats"))
        out.osd.osd_show_gs_stats = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_cpu"))
        out.osd.osd_show_cpu = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_gpu"))
        out.osd.osd_show_gpu = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_indicators"))
        out.osd.osd_show_indicators = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_frame_times"))
        out.osd.osd_show_frame_times = parse_bool(v);

    if (const char* v = query("pcsx2_osd_show_hardware_info"))
        out.osd.osd_show_hardware_info = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_version"))
        out.osd.osd_show_version = parse_bool(v);

    if (const char* v = query("pcsx2_osd_show_settings"))
        out.osd.osd_show_settings = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_patches"))
        out.osd.osd_show_patches = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_inputs"))
        out.osd.osd_show_inputs = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_video_capture"))
        out.osd.osd_show_video_capture = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_input_rec"))
        out.osd.osd_show_input_rec = parse_bool(v);
    if (const char* v = query("pcsx2_osd_show_texture_replacements"))
        out.osd.osd_show_texture_replacements = parse_bool(v);

    if (const char* v = query("pcsx2_warn_about_unsafe_settings"))
        out.osd.warn_about_unsafe_settings = parse_bool(v);
}

#ifndef CORE_OPTIONS_TEST_ONLY
void ApplyDefaults(MemorySettingsInterface& si, const Values& v)
{
    // ── Display sub-tab ──
    // String-valued combos (aspect-ratio rows) write the INI value
    // verbatim — the libretro stored value IS the INI string.
    si.SetStringValue("EmuCore/GS", "AspectRatio",          v.display.aspect_ratio.c_str());
    si.SetStringValue("EmuCore/GS", "FMVAspectRatioSwitch", v.display.fmv_aspect_ratio.c_str());

    // Int-valued combos.
    si.SetIntValue("EmuCore/GS", "deinterlace_mode",    v.display.deinterlace_mode);
    si.SetIntValue("EmuCore/GS", "linear_present_mode", v.display.linear_present_mode);

    // Int-as-Combo (slider equivalents).
    si.SetIntValue("EmuCore/GS", "StretchY",   v.display.stretch_y);
    si.SetIntValue("EmuCore/GS", "CropLeft",   v.display.crop_left);
    si.SetIntValue("EmuCore/GS", "CropTop",    v.display.crop_top);
    si.SetIntValue("EmuCore/GS", "CropRight",  v.display.crop_right);
    si.SetIntValue("EmuCore/GS", "CropBottom", v.display.crop_bottom);

    // Patches live under [EmuCore] (not [EmuCore/GS]).
    si.SetBoolValue("EmuCore", "EnableWideScreenPatches",     v.display.enable_widescreen_patches);
    si.SetBoolValue("EmuCore", "EnableNoInterlacingPatches",  v.display.enable_no_interlacing_patches);

    // GS-side display checkboxes.
    si.SetBoolValue("EmuCore/GS", "pcrtc_antiblur",           v.display.pcrtc_antiblur);
    si.SetBoolValue("EmuCore/GS", "IntegerScaling",           v.display.integer_scaling);
    si.SetBoolValue("EmuCore/GS", "pcrtc_offsets",            v.display.pcrtc_offsets);
    si.SetBoolValue("EmuCore/GS", "disable_interlace_offset", v.display.disable_interlace_offset);
    si.SetBoolValue("EmuCore/GS", "pcrtc_overscan",           v.display.pcrtc_overscan);

    // ── Rendering sub-tab ──
    si.SetIntValue ("EmuCore/GS", "upscale_multiplier",     v.rendering.upscale_multiplier);
    si.SetIntValue ("EmuCore/GS", "filter",                 v.rendering.filter);
    si.SetIntValue ("EmuCore/GS", "TriFilter",              v.rendering.tri_filter);
    si.SetIntValue ("EmuCore/GS", "MaxAnisotropy",          v.rendering.max_anisotropy);
    si.SetIntValue ("EmuCore/GS", "dithering_ps2",          v.rendering.dithering_ps2);
    si.SetIntValue ("EmuCore/GS", "accurate_blending_unit", v.rendering.accurate_blending_unit);
    si.SetBoolValue("EmuCore/GS", "hw_mipmap",              v.rendering.hw_mipmap);

    // ── Texture Replacement sub-tab ──
    si.SetBoolValue("EmuCore/GS", "LoadTextureReplacements",      v.texture_replacement.load_texture_replacements);
    si.SetBoolValue("EmuCore/GS", "DumpReplaceableTextures",      v.texture_replacement.dump_replaceable_textures);
    si.SetBoolValue("EmuCore/GS", "LoadTextureReplacementsAsync", v.texture_replacement.load_texture_replacements_async);
    si.SetBoolValue("EmuCore/GS", "PrecacheTextureReplacements",  v.texture_replacement.precache_texture_replacements);
    si.SetBoolValue("EmuCore/GS", "DumpReplaceableMipmaps",       v.texture_replacement.dump_replaceable_mipmaps);
    si.SetBoolValue("EmuCore/GS", "DumpTexturesWithFMVActive",    v.texture_replacement.dump_textures_with_fmv_active);

    // ── Post-Processing sub-tab ──
    si.SetIntValue ("EmuCore/GS", "CASMode",               v.post_processing.cas_mode);
    si.SetIntValue ("EmuCore/GS", "CASSharpness",          v.post_processing.cas_sharpness);
    si.SetBoolValue("EmuCore/GS", "fxaa",                  v.post_processing.fxaa);
    si.SetIntValue ("EmuCore/GS", "TVShader",              v.post_processing.tv_shader);
    si.SetBoolValue("EmuCore/GS", "ShadeBoost",            v.post_processing.shade_boost);
    si.SetIntValue ("EmuCore/GS", "ShadeBoost_Brightness", v.post_processing.shade_boost_brightness);
    si.SetIntValue ("EmuCore/GS", "ShadeBoost_Contrast",   v.post_processing.shade_boost_contrast);
    si.SetIntValue ("EmuCore/GS", "ShadeBoost_Saturation", v.post_processing.shade_boost_saturation);
    si.SetIntValue ("EmuCore/GS", "ShadeBoost_Gamma",      v.post_processing.shade_boost_gamma);

    // ── On-Screen Display sub-tab ──
    // 22 fields under [EmuCore/GS]; warn_about_unsafe_settings under
    // [EmuCore] (split-section, see spec). OsdshowPatches INI key
    // preserves PCSX2's lowercase-'s' typo verbatim (Config.h:781) —
    // mis-spelling here would silently no-op (PCSX2 ignores the
    // correctly-cased key on read).
    si.SetIntValue ("EmuCore/GS", "OsdScale",                   v.osd.osd_scale);
    si.SetIntValue ("EmuCore/GS", "OsdMargin",                  v.osd.osd_margin);
    si.SetIntValue ("EmuCore/GS", "OsdMessagesPos",             v.osd.osd_messages_pos);
    si.SetIntValue ("EmuCore/GS", "OsdPerformancePos",          v.osd.osd_performance_pos);
    si.SetBoolValue("EmuCore/GS", "OsdBoldText",                v.osd.osd_bold_text);

    si.SetBoolValue("EmuCore/GS", "OsdShowSpeed",               v.osd.osd_show_speed);
    si.SetBoolValue("EmuCore/GS", "OsdShowFPS",                 v.osd.osd_show_fps);
    si.SetBoolValue("EmuCore/GS", "OsdShowVPS",                 v.osd.osd_show_vps);
    si.SetBoolValue("EmuCore/GS", "OsdShowResolution",          v.osd.osd_show_resolution);
    si.SetBoolValue("EmuCore/GS", "OsdShowGSStats",             v.osd.osd_show_gs_stats);
    si.SetBoolValue("EmuCore/GS", "OsdShowCPU",                 v.osd.osd_show_cpu);
    si.SetBoolValue("EmuCore/GS", "OsdShowGPU",                 v.osd.osd_show_gpu);
    si.SetBoolValue("EmuCore/GS", "OsdShowIndicators",          v.osd.osd_show_indicators);
    si.SetBoolValue("EmuCore/GS", "OsdShowFrameTimes",          v.osd.osd_show_frame_times);

    si.SetBoolValue("EmuCore/GS", "OsdShowHardwareInfo",        v.osd.osd_show_hardware_info);
    si.SetBoolValue("EmuCore/GS", "OsdShowVersion",             v.osd.osd_show_version);

    si.SetBoolValue("EmuCore/GS", "OsdShowSettings",            v.osd.osd_show_settings);
    si.SetBoolValue("EmuCore/GS", "OsdshowPatches",             v.osd.osd_show_patches); // lowercase-s, see Config.h:781
    si.SetBoolValue("EmuCore/GS", "OsdShowInputs",              v.osd.osd_show_inputs);
    si.SetBoolValue("EmuCore/GS", "OsdShowVideoCapture",        v.osd.osd_show_video_capture);
    si.SetBoolValue("EmuCore/GS", "OsdShowInputRec",            v.osd.osd_show_input_rec);
    si.SetBoolValue("EmuCore/GS", "OsdShowTextureReplacements", v.osd.osd_show_texture_replacements);

    // Split section: WarnAboutUnsafeSettings lives in [EmuCore], not
    // [EmuCore/GS]. Matches Pcsx2Config.cpp:1943 placement.
    si.SetBoolValue("EmuCore",    "WarnAboutUnsafeSettings",    v.osd.warn_about_unsafe_settings);
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::Graphics
