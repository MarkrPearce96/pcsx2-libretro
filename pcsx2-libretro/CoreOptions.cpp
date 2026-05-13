// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptions.h"
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
#include "LibretroFrontend.h"
#endif

#include <cstring>

namespace Pcsx2Libretro::CoreOptions
{

const std::vector<retro_core_option_v2_definition>& BuildDefinitions()
{
    // Function-local static — initialized once on first call, lives for the
    // process lifetime, addresses are stable. libretro's SET_CORE_OPTIONS_V2
    // requires the definitions array (and the strings it points at) to
    // remain valid until retro_deinit. The strings are all literal — static
    // by construction. The array storage lives here.
    static const std::vector<retro_core_option_v2_definition> kAll = [] {
        std::vector<retro_core_option_v2_definition> v;
        v.reserve(8);  // tiny pre-reserve; future phases expand this.
        Emulation::AppendDefinitions(v);
        // libretro terminator — must be the final entry per libretro.h:6787.
        v.push_back({
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            {{nullptr, nullptr}},
            nullptr
        });
        return v;
    }();
    return kAll;
}

bool EmitCoreOptionsV2(retro_environment_t cb)
{
    if (!cb) return false;

    // SET_CORE_OPTIONS_V2 wants a retro_core_options_v2 (categories +
    // definitions). categories=nullptr → uncategorized; RetroNest's host
    // adapter places these under SettingDef.category on its side.
    retro_core_options_v2 opts{};
    opts.categories  = nullptr;
    opts.definitions = const_cast<retro_core_option_v2_definition*>(
        BuildDefinitions().data());

    const bool ok = cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &opts);
    if (!ok) {
        // Per libretro.h:2340, false means the host doesn't support option
        // CATEGORIES — options themselves are still registered and
        // GET_VARIABLE will work. We pass categories=nullptr anyway, so this
        // is purely informational; user values still flow.
        FrontendLog(RETRO_LOG_WARN,
            "[CoreOptions] Host does not support core-option categories "
            "(options are still registered and GET_VARIABLE will work)");
    }
    return ok;
}

Resolved ReadResolved(retro_environment_t cb)
{
    Resolved r{};
    if (!cb) return r;

    Emulation::Parse(cb, r.emulation);

    // Future phases append Graphics::Parse, Audio::Parse, MemoryCards::Parse here.

    // SP7c Phase 1 followup: echo all resolved Emulation values so smoke
    // testing can verify a knob's value actually reached the core (PCSX2
    // itself doesn't echo Speedhacks/Framerate/GS values to stderr).
    // 4 lines grouped by sub-group so a grep "[CoreOptions]" gives a
    // compact per-launch snapshot.
    const auto& e = r.emulation;
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] renderer=%d mtvu=%s fast_boot=%s",
        e.renderer, e.mtvu ? "on" : "off", e.fast_boot ? "on" : "off");
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] speed: normal=%.3f ff=%.3f slomo=%.3f",
        e.normal_speed, e.fast_forward_speed, e.slow_motion_speed);
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] system: ee_rate=%d ee_skip=%d thread_pin=%s "
        "cheats=%s host_fs=%s cdvd_precache=%s fast_boot_ff=%s",
        e.ee_cycle_rate, e.ee_cycle_skip,
        e.thread_pinning ? "on" : "off",
        e.cheats ? "on" : "off",
        e.host_fs ? "on" : "off",
        e.cdvd_precache ? "on" : "off",
        e.fast_boot_ff ? "on" : "off");
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] pacing: queue=%d host_rr=%s vsync=%s "
        "vsync_timing=%s skip_dup=%s",
        e.vsync_queue_size,
        e.sync_to_host_rr ? "on" : "off",
        e.vsync ? "on" : "off",
        e.use_vsync_timing ? "on" : "off",
        e.skip_duplicate_frames ? "on" : "off");

    return r;
}

} // namespace Pcsx2Libretro::CoreOptions
