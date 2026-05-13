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
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::Graphics
