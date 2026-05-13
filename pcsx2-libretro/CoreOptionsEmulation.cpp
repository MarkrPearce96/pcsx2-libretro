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

#include <cstdlib>
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

    // SP7c Phase 1 — Speed Control sub-group.
    //
    // All three scalars share the same 17-preset value list. Labels are
    // copied verbatim from the standalone PCSX2 dialog
    // (RetroNest-Project/cpp/src/adapters/pcsx2_adapter.cpp:200-218) so
    // users see identical wording between the two PCSX2 variants.
    //
    // INI form: PCSX2 writes via StringUtil::ToChars (shortest-form). "1"
    // not "1.0", "0.5" not "0.50". Our option values must match this exact
    // form to round-trip cleanly.
    //
    // The three blocks are written as literal `out.push_back({...})`
    // brace-init lists (NOT a lambda helper) so tools/check_schema_fidelity.py's
    // CORE_BLOCK_RE recognizes them; the script is the source of truth
    // for byte-for-byte parity with the host adapter.
    out.push_back({
        "pcsx2_normal_speed",
        "Normal Speed",
        nullptr,
        "Target emulation speed during normal gameplay (relative to PS2's "
        "native rate). 100% is real-time. Lower for handheld-style throttling, "
        "higher for fast-forwarding through cutscenes.",
        nullptr,
        nullptr,
        {
            { "0.02", "2% [1 FPS (NTSC) / 1 FPS (PAL)]" },
            { "0.1",  "10% [6 FPS (NTSC) / 5 FPS (PAL)]" },
            { "0.25", "25% [15 FPS (NTSC) / 12 FPS (PAL)]" },
            { "0.5",  "50% [30 FPS (NTSC) / 25 FPS (PAL)]" },
            { "0.75", "75% [45 FPS (NTSC) / 37 FPS (PAL)]" },
            { "0.9",  "90% [54 FPS (NTSC) / 45 FPS (PAL)]" },
            { "1",    "100% [60 FPS (NTSC) / 50 FPS (PAL)]" },
            { "1.1",  "110% [66 FPS (NTSC) / 55 FPS (PAL)]" },
            { "1.2",  "120% [72 FPS (NTSC) / 60 FPS (PAL)]" },
            { "1.5",  "150% [90 FPS (NTSC) / 75 FPS (PAL)]" },
            { "1.75", "175% [105 FPS (NTSC) / 87 FPS (PAL)]" },
            { "2",    "200% [120 FPS (NTSC) / 100 FPS (PAL)]" },
            { "3",    "300% [180 FPS (NTSC) / 150 FPS (PAL)]" },
            { "4",    "400% [240 FPS (NTSC) / 200 FPS (PAL)]" },
            { "5",    "500% [300 FPS (NTSC) / 250 FPS (PAL)]" },
            { "10",   "1000% [600 FPS (NTSC) / 500 FPS (PAL)]" },
            { "0",    "Unlimited" },
            { nullptr, nullptr },
        },
        "1",
    });

    out.push_back({
        "pcsx2_fast_forward_speed",
        "Fast-Forward Speed",
        nullptr,
        "Target speed when fast-forward is engaged. The frontend's "
        "fast-forward toggle scales emulation by this factor.",
        nullptr,
        nullptr,
        {
            { "0.02", "2% [1 FPS (NTSC) / 1 FPS (PAL)]" },
            { "0.1",  "10% [6 FPS (NTSC) / 5 FPS (PAL)]" },
            { "0.25", "25% [15 FPS (NTSC) / 12 FPS (PAL)]" },
            { "0.5",  "50% [30 FPS (NTSC) / 25 FPS (PAL)]" },
            { "0.75", "75% [45 FPS (NTSC) / 37 FPS (PAL)]" },
            { "0.9",  "90% [54 FPS (NTSC) / 45 FPS (PAL)]" },
            { "1",    "100% [60 FPS (NTSC) / 50 FPS (PAL)]" },
            { "1.1",  "110% [66 FPS (NTSC) / 55 FPS (PAL)]" },
            { "1.2",  "120% [72 FPS (NTSC) / 60 FPS (PAL)]" },
            { "1.5",  "150% [90 FPS (NTSC) / 75 FPS (PAL)]" },
            { "1.75", "175% [105 FPS (NTSC) / 87 FPS (PAL)]" },
            { "2",    "200% [120 FPS (NTSC) / 100 FPS (PAL)]" },
            { "3",    "300% [180 FPS (NTSC) / 150 FPS (PAL)]" },
            { "4",    "400% [240 FPS (NTSC) / 200 FPS (PAL)]" },
            { "5",    "500% [300 FPS (NTSC) / 250 FPS (PAL)]" },
            { "10",   "1000% [600 FPS (NTSC) / 500 FPS (PAL)]" },
            { "0",    "Unlimited" },
            { nullptr, nullptr },
        },
        "2",
    });

    out.push_back({
        "pcsx2_slow_motion_speed",
        "Slow-Motion Speed",
        nullptr,
        "Target speed when slow-motion is engaged. The frontend's "
        "slow-motion toggle scales emulation by this factor.",
        nullptr,
        nullptr,
        {
            { "0.02", "2% [1 FPS (NTSC) / 1 FPS (PAL)]" },
            { "0.1",  "10% [6 FPS (NTSC) / 5 FPS (PAL)]" },
            { "0.25", "25% [15 FPS (NTSC) / 12 FPS (PAL)]" },
            { "0.5",  "50% [30 FPS (NTSC) / 25 FPS (PAL)]" },
            { "0.75", "75% [45 FPS (NTSC) / 37 FPS (PAL)]" },
            { "0.9",  "90% [54 FPS (NTSC) / 45 FPS (PAL)]" },
            { "1",    "100% [60 FPS (NTSC) / 50 FPS (PAL)]" },
            { "1.1",  "110% [66 FPS (NTSC) / 55 FPS (PAL)]" },
            { "1.2",  "120% [72 FPS (NTSC) / 60 FPS (PAL)]" },
            { "1.5",  "150% [90 FPS (NTSC) / 75 FPS (PAL)]" },
            { "1.75", "175% [105 FPS (NTSC) / 87 FPS (PAL)]" },
            { "2",    "200% [120 FPS (NTSC) / 100 FPS (PAL)]" },
            { "3",    "300% [180 FPS (NTSC) / 150 FPS (PAL)]" },
            { "4",    "400% [240 FPS (NTSC) / 200 FPS (PAL)]" },
            { "5",    "500% [300 FPS (NTSC) / 250 FPS (PAL)]" },
            { "10",   "1000% [600 FPS (NTSC) / 500 FPS (PAL)]" },
            { "0",    "Unlimited" },
            { nullptr, nullptr },
        },
        "0.5",
    });

    // SP7c Phase 1 — System Settings sub-group.
    //
    // Each entry uses a literal out.push_back({...}) form so
    // tools/check_schema_fidelity.py's CORE_BLOCK_RE can parse it
    // (the regex doesn't expand lambda or helper-function emissions).
    // For the 5 bool knobs the "enabled/disabled" value list is repeated
    // inline — minor duplication but keeps the schema-fidelity contract
    // honest.
    out.push_back({
        "pcsx2_ee_cycle_rate",
        "EE Cycle Rate",
        nullptr,
        "Underclocks or overclocks the emulated Emotion Engine CPU. "
        "Negative values reduce work-per-cycle (compatibility/perf tradeoff); "
        "positive values speed CPU-bound games at the cost of timing accuracy. "
        "Most games should stay at 100%.",
        nullptr,
        nullptr,
        {
            { "-3", "50% (Underclock)" },
            { "-2", "60% (Underclock)" },
            { "-1", "75% (Underclock)" },
            { "0",  "100% (Normal Speed)" },
            { "1",  "130% (Overclock)" },
            { "2",  "180% (Overclock)" },
            { "3",  "300% (Overclock)" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_ee_cycle_skip",
        "EE Cycle Skipping",
        nullptr,
        "Makes the emulated Emotion Engine skip cycles. Stronger underclock "
        "than EE Cycle Rate; can recover frame-rate in slow scenes at the "
        "cost of visible glitches. Leave Disabled unless a specific game "
        "needs it.",
        nullptr,
        nullptr,
        {
            { "0", "Disabled" },
            { "1", "Mild Underclock" },
            { "2", "Moderate Underclock" },
            { "3", "Maximum Underclock" },
            { nullptr, nullptr },
        },
        "0",
    });

    out.push_back({
        "pcsx2_thread_pinning",
        "Thread Pinning",
        nullptr,
        "Pin emulation threads to specific CPU cores. Can reduce stutter "
        "on systems with heterogeneous cores (e.g. Apple Silicon "
        "performance/efficiency split). Default off.",
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
        "pcsx2_cheats",
        "Enable Cheats",
        nullptr,
        "Load pnach cheat files from the cheats/ resource directory on game "
        "launch. Off by default; the in-game OSD logs each loaded patch line.",
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
        "pcsx2_host_fs",
        "Enable Host Filesystem",
        nullptr,
        "Allow the emulated PS2 to read files from the host filesystem "
        "(homebrew-only feature; retail games never use it). Off by default.",
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
        "pcsx2_cdvd_precache",
        "CDVD Precache",
        nullptr,
        "Load the entire disc image into RAM before booting. Eliminates "
        "in-game disc-read stutter at the cost of extra memory usage and a "
        "slower initial boot. Off by default.",
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
        "pcsx2_fast_boot_ff",
        "Fast-Forward Through BIOS",
        nullptr,
        "When Fast Boot is enabled, also auto-fast-forward the brief BIOS "
        "boot animation. Has no effect when Fast Boot is disabled.",
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

    if (const char* v = query("pcsx2_renderer")) {
        if      (std::strcmp(v, "auto")     == 0) out.renderer = -1;
        else if (std::strcmp(v, "metal")    == 0) out.renderer = 17;
        else if (std::strcmp(v, "software") == 0) out.renderer = 13;
        else if (std::strcmp(v, "null")     == 0) out.renderer = 11;
        else {
            FrontendLog(RETRO_LOG_WARN,
                "[CoreOptions] Unknown renderer '%s'; defaulting to auto", v);
            out.renderer = -1;
        }
    }

    if (const char* v = query("pcsx2_mtvu"))
        out.mtvu = (std::strcmp(v, "enabled") == 0);

    if (const char* v = query("pcsx2_fast_boot"))
        out.fast_boot = (std::strcmp(v, "enabled") == 0);

    // SP7c Phase 1 — Speed Control parsing.
    // Values are float-encoded strings. strtof handles the canonical
    // "1", "0.5", "2", "10", "0" (Unlimited) forms. On bad parse, leave
    // the field at its Values{} default and WARN.
    auto parse_speed = [&query](const char* key, float& out_field, float fallback) {
        if (const char* v = query(key)) {
            char* end = nullptr;
            const float parsed = std::strtof(v, &end);
            if (end == v) {
                FrontendLog(RETRO_LOG_WARN,
                    "[CoreOptions] Unparseable %s='%s'; keeping default %.3f",
                    key, v, fallback);
                out_field = fallback;
            } else {
                out_field = parsed;
            }
        }
    };

    parse_speed("pcsx2_normal_speed",       out.normal_speed,       1.0f);
    parse_speed("pcsx2_fast_forward_speed", out.fast_forward_speed, 2.0f);
    parse_speed("pcsx2_slow_motion_speed",  out.slow_motion_speed,  0.5f);

    // SP7c Phase 1 — System Settings parsing.
    auto parse_int = [&query](const char* key, int& out_field, int fallback) {
        if (const char* v = query(key)) {
            char* end = nullptr;
            const long parsed = std::strtol(v, &end, 10);
            if (end == v) {
                FrontendLog(RETRO_LOG_WARN,
                    "[CoreOptions] Unparseable %s='%s'; keeping default %d",
                    key, v, fallback);
                out_field = fallback;
            } else {
                out_field = static_cast<int>(parsed);
            }
        }
    };

    auto parse_bool = [&query](const char* key, bool& out_field) {
        if (const char* v = query(key))
            out_field = (std::strcmp(v, "enabled") == 0);
    };

    parse_int("pcsx2_ee_cycle_rate", out.ee_cycle_rate, 0);
    parse_int("pcsx2_ee_cycle_skip", out.ee_cycle_skip, 0);
    parse_bool("pcsx2_thread_pinning", out.thread_pinning);
    parse_bool("pcsx2_cheats",         out.cheats);
    parse_bool("pcsx2_host_fs",        out.host_fs);
    parse_bool("pcsx2_cdvd_precache",  out.cdvd_precache);
    parse_bool("pcsx2_fast_boot_ff",   out.fast_boot_ff);
}

#ifndef CORE_OPTIONS_TEST_ONLY
// Body gated because MemorySettingsInterface's SetIntValue/SetBoolValue
// aren't available in the standalone-test compile chain. test_core_options
// never calls ApplyDefaults (it tests Parse + Emit only); the apply path
// is exercised at the live-smoke level via Settings::InitializeDefaults.
void ApplyDefaults(MemorySettingsInterface& si, const Values& v)
{
    // SP3: Renderer was Null (11) at first to bring up without a display
    // surface; SP3 added Pattern B with a real CAMetalLayer so Auto (-1)
    // works. Supported per pcsx2/Config.h:271-281:
    //   Auto = -1, Null = 11, SW = 13, Metal = 17.
    si.SetIntValue("EmuCore/GS", "Renderer", v.renderer);

    // Fast boot — also wired in LibretroFrontend.cpp via
    // VMBootParameters.fast_boot, which overrides this INI at boot time.
    // Both layers MUST get the same value.
    si.SetBoolValue("EmuCore", "EnableFastBoot", v.fast_boot);

    // Multi-Threaded VU1 — default on (SP5 perf rationale: Apple Silicon
    // interpreters saturate the EE thread otherwise). Disable only for
    // games with documented MTVU glitches.
    si.SetBoolValue("EmuCore/Speedhacks", "vuThread", v.mtvu);

    // SP7c Phase 1 — Speed Control. PCSX2's Framerate section reads these
    // floats directly into Pcsx2Config; SetFloatValue routes through
    // ToChars so the on-disk INI string matches our option-values list
    // ("1", "0.5", "2", "0" for Unlimited).
    si.SetFloatValue("Framerate", "NominalScalar", v.normal_speed);
    si.SetFloatValue("Framerate", "TurboScalar",   v.fast_forward_speed);
    si.SetFloatValue("Framerate", "SlomoScalar",   v.slow_motion_speed);

    // SP7c Phase 1 — System Settings.
    si.SetIntValue ("EmuCore/Speedhacks", "EECycleRate", v.ee_cycle_rate);
    si.SetIntValue ("EmuCore/Speedhacks", "EECycleSkip", v.ee_cycle_skip);
    si.SetBoolValue("EmuCore", "EnableThreadPinning",       v.thread_pinning);
    si.SetBoolValue("EmuCore", "EnableCheats",              v.cheats);
    si.SetBoolValue("EmuCore", "HostFs",                    v.host_fs);
    si.SetBoolValue("EmuCore", "CdvdPrecache",              v.cdvd_precache);
    si.SetBoolValue("EmuCore", "EnableFastBootFastForward", v.fast_boot_ff);
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::Emulation
