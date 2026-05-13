// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptions.h"
#include "CoreOptionsEmulation.h"
#include "CoreOptionsAudio.h"
#include "CoreOptionsMemoryCards.h"
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
        v.reserve(96);  // 18 Phase 1 + 5 Phase 2 + 5 Phase 3 + 62 Phase 4 + terminator + headroom
        Emulation::AppendDefinitions(v);
        Audio::AppendDefinitions(v);
        MemoryCards::AppendDefinitions(v);
        Graphics::AppendDefinitions(v);
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
    Audio::Parse(cb, r.audio);
    MemoryCards::Parse(cb, r.memory_cards);
    Graphics::Parse(cb, r.graphics);

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

    const auto& a = r.audio;
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] audio: sync_mode=%s buffer_ms=%d volume=%d "
        "ff_volume=%d muted=%s",
        a.sync_mode.c_str(), a.buffer_ms, a.volume, a.ff_volume,
        a.muted ? "on" : "off");

    const auto& m = r.memory_cards;
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] memory_cards: slot1=%s slot2=%s mt1_s2=%s "
        "mt1_s3=%s mt1_s4=%s",
        m.slot1_enable    ? "on" : "off",
        m.slot2_enable    ? "on" : "off",
        m.multitap1_slot2 ? "on" : "off",
        m.multitap1_slot3 ? "on" : "off",
        m.multitap1_slot4 ? "on" : "off");

    // SP7c Phase 4: one echo line per Graphics sub-tab. Each sub-tab
    // task extends ReadResolved's log with its own line so smoke can
    // verify every Graphics knob reached the core. Phase 4 Task 2
    // landed Display; later tasks add rendering/texrep/postproc/osd.
    const auto& gd = r.graphics.display;
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] graphics.display: ar=%s fmv_ar=%s deint=%d "
        "bilinear=%d stretchY=%d crop=%d/%d/%d/%d ws_patch=%s "
        "no_int_patch=%s antiblur=%s int_scale=%s offsets=%s "
        "no_int_off=%s overscan=%s",
        gd.aspect_ratio.c_str(), gd.fmv_aspect_ratio.c_str(),
        gd.deinterlace_mode, gd.linear_present_mode, gd.stretch_y,
        gd.crop_left, gd.crop_top, gd.crop_right, gd.crop_bottom,
        gd.enable_widescreen_patches    ? "on" : "off",
        gd.enable_no_interlacing_patches ? "on" : "off",
        gd.pcrtc_antiblur               ? "on" : "off",
        gd.integer_scaling              ? "on" : "off",
        gd.pcrtc_offsets                ? "on" : "off",
        gd.disable_interlace_offset     ? "on" : "off",
        gd.pcrtc_overscan               ? "on" : "off");

    const auto& gr = r.graphics.rendering;
    FrontendLog(RETRO_LOG_INFO,
        "[CoreOptions] graphics.rendering: upscale=%d filter=%d "
        "trifilter=%d maxaniso=%d dither=%d blendaccu=%d mipmap=%s",
        gr.upscale_multiplier, gr.filter, gr.tri_filter,
        gr.max_anisotropy, gr.dithering_ps2, gr.accurate_blending_unit,
        gr.hw_mipmap ? "on" : "off");

    return r;
}

} // namespace Pcsx2Libretro::CoreOptions
