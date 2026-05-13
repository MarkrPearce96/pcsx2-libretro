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
    // Filled in Task 3.
    (void)out;
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
