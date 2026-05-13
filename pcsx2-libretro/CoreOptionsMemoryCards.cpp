// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsMemoryCards.h"

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

#include <cstring>

namespace Pcsx2Libretro::CoreOptions::MemoryCards
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& /*out*/)
{
    // Filled in by Task 2.
}

void Parse(retro_environment_t /*cb*/, Values& /*out*/)
{
    // Filled in by Task 2.
}

#ifndef CORE_OPTIONS_TEST_ONLY
void ApplyDefaults(MemorySettingsInterface& /*si*/, const Values& /*v*/)
{
    // Filled in by Task 2.
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::MemoryCards
