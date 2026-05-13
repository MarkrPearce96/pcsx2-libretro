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
