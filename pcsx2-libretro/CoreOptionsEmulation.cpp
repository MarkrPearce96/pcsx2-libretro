// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsEmulation.h"

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
#include "common/MemorySettingsInterface.h"    // MemorySettingsInterface + SettingsInterface base
#endif

#include <cstring>

namespace Pcsx2Libretro::CoreOptions::Emulation
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)
{
    // Field order per libretro.h:6646-6763:
    //   key, desc, desc_categorized, info, info_categorized, category_key,
    //   values[RETRO_NUM_CORE_OPTION_VALUES_MAX], default_value.
    //
    // NULL for desc_categorized/info_categorized/category_key tells the
    // frontend to display these uncategorized — RetroNest places them
    // under SettingDef.category on the host side.
    out.push_back({
        "pcsx2_renderer",
        "GS Renderer",
        nullptr,
        "PCSX2 graphics backend. Auto picks Metal on macOS. "
        "Software runs on CPU only (much slower; useful for debugging "
        "rendering bugs or for games with hardware-renderer regressions).",
        nullptr,
        nullptr,
        {
            { "auto",     "Auto" },
            { "metal",    "Metal" },
            { "software", "Software" },
            { "null",     "Null" },
            { nullptr,    nullptr },
        },
        "auto",
    });

    out.push_back({
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
    });

    out.push_back({
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
    });
}

void Parse(retro_environment_t cb, Values& out)
{
    // Filled in Task 4.
    (void)cb;
    (void)out;
}

#ifndef CORE_OPTIONS_TEST_ONLY
// Body gated because MemorySettingsInterface's SetIntValue/SetBoolValue
// aren't available in the standalone-test compile chain. test_core_options
// never calls ApplyDefaults (it tests Parse + Emit only); the apply path
// is exercised at the live-smoke level via Settings::InitializeDefaults.
void ApplyDefaults(MemorySettingsInterface& si, const Values& v)
{
    // Filled in Task 5.
    (void)si;
    (void)v;
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::Emulation
