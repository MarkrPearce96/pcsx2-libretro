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
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::Graphics
