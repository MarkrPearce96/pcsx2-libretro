// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptions.h"

#ifdef SP7B_TEST_CORE_OPTIONS_ONLY
// Standalone test mode: stub FrontendLog so this compiles without
// the rest of pcsx2-libretro. The retro_log_level enum still comes
// from libretro.h (included via CoreOptions.h).
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
#include "LibretroFrontend.h"  // FrontendLog
#endif

#include <cstring>

namespace
{

// Option schema. Field order per libretro.h:6646-6763:
//   key, desc, desc_categorized, info, info_categorized, category_key,
//   values[], default_value.
//
// Terminator: a fully-zeroed entry (the libretro spec requires this).
//
// Note: NULL for desc_categorized/info_categorized/category_key tells
// the frontend to display these options uncategorized — RetroNest places
// them under its own "Recommended" tab via SettingDef.category in the
// host adapter.
const retro_core_option_v2_definition kDefinitions[] = {
    {
        "pcsx2_renderer",
        "GS Renderer",
        nullptr,                          // desc_categorized
        "PCSX2 graphics backend. Auto picks Metal on macOS. "
        "Software runs on CPU only (much slower; useful for debugging "
        "rendering bugs or for games with hardware-renderer regressions).",
        nullptr,                          // info_categorized
        nullptr,                          // category_key
        {
            { "auto",     "Auto" },
            { "metal",    "Metal" },
            { "software", "Software" },
            { "null",     "Null" },
            { nullptr,    nullptr },      // terminator
        },
        "auto",                           // default_value
    },
    {
        "pcsx2_mtvu",
        "Multi-Threaded VU1",
        nullptr,
        "Run the VU1 microprogram on its own thread instead of the EE thread. "
        "Compatible with the vast majority of games; significantly reduces "
        "EE-thread saturation on Apple Silicon's interpreter-only path. "
        "Disable only if a specific game shows MTVU-related glitches.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    },
    {
        "pcsx2_fast_boot",
        "Fast Boot",
        nullptr,
        "Skip the PS2 BIOS Sony intro and region-check screen on launch. "
        "Disable if you want to see the BIOS screen (e.g. to verify your "
        "BIOS region or to use the BIOS browser).",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    },
    // Terminator — zeroed entry per libretro.h:6787.
    { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, {{nullptr,nullptr}}, nullptr },
};

const retro_core_options_v2 kCoreOptionsV2 = {
    nullptr,                              // categories — uncategorized
    const_cast<retro_core_option_v2_definition*>(kDefinitions),
};

} // namespace

namespace Pcsx2Libretro::CoreOptions
{

bool EmitCoreOptionsV2(retro_environment_t cb)
{
    if (!cb) return false;
    const bool ok = cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
                       const_cast<retro_core_options_v2*>(&kCoreOptionsV2));
    if (!ok) {
        FrontendLog(RETRO_LOG_WARN,
            "[CoreOptions] SET_CORE_OPTIONS_V2 not supported by host; "
            "core will use built-in defaults");
    }
    return ok;
}

Resolved ReadResolved(retro_environment_t cb)
{
    Resolved r{};  // defaults: renderer=-1, mtvu=true, fast_boot=true
    if (!cb) return r;

    auto query = [&cb](const char* key) -> const char* {
        retro_variable var{};
        var.key = key;
        if (cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
            return var.value;
        return nullptr;
    };

    if (const char* v = query("pcsx2_renderer")) {
        if      (std::strcmp(v, "auto")     == 0) r.renderer = -1;
        else if (std::strcmp(v, "metal")    == 0) r.renderer = 17;
        else if (std::strcmp(v, "software") == 0) r.renderer = 13;
        else if (std::strcmp(v, "null")     == 0) r.renderer = 11;
        else {
            FrontendLog(RETRO_LOG_WARN,
                "[CoreOptions] Unknown renderer '%s'; defaulting to auto", v);
            r.renderer = -1;
        }
    }

    if (const char* v = query("pcsx2_mtvu"))
        r.mtvu = (std::strcmp(v, "enabled") == 0);

    if (const char* v = query("pcsx2_fast_boot"))
        r.fast_boot = (std::strcmp(v, "enabled") == 0);

    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] renderer=%d mtvu=%s fast_boot=%s",
        r.renderer, r.mtvu ? "on" : "off", r.fast_boot ? "on" : "off");

    return r;
}

} // namespace Pcsx2Libretro::CoreOptions
