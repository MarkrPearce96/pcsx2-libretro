// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsAudio.h"

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

namespace Pcsx2Libretro::CoreOptions::Audio
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& /*out*/)
{
    // SP7c Phase 2 Task 2 fills this in with 5 literal out.push_back({...})
    // blocks. Empty in Task 1 to verify the aggregator plumbing first.
}

void Parse(retro_environment_t /*cb*/, Values& /*out*/)
{
    // SP7c Phase 2 Task 2 fills this in with 5 parse branches.
}

#ifndef CORE_OPTIONS_TEST_ONLY
void ApplyDefaults(MemorySettingsInterface& /*si*/, const Values& /*v*/)
{
    // SP7c Phase 2 Task 2 fills this in with 5 si.SetXValue(...) lines.
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::Audio
