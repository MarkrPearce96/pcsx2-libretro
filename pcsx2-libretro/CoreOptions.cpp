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

namespace Pcsx2Libretro::CoreOptions
{

bool EmitCoreOptionsV2(retro_environment_t /*cb*/)
{
    // Task 4: real implementation lands here. Stub returns false so
    // callers fall through to compile-time defaults during scaffolding.
    return false;
}

Resolved ReadResolved(retro_environment_t /*cb*/)
{
    // Task 2/3: real implementation lands here. Stub returns built-in
    // defaults so the rest of the system behaves exactly like SP7a.
    return Resolved{};
}

} // namespace Pcsx2Libretro::CoreOptions
