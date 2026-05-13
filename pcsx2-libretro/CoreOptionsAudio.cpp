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

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)
{
    // SP7c Phase 2 — Audio card (5 knobs under SPU2/Output).
    //
    // Why these 5 and not the standalone's 11: audio routes through
    // LibretroAudioStream (SP4), so Cubeb-only knobs (Backend, DriverName,
    // DeviceName, ExpansionMode, OutputLatencyMS, OutputLatencyMinimal) are
    // silently inert and dropped. The 5 retained knobs each have a verified
    // consumer in libretro mode — see plan section "Knob inventory" for the
    // line-level cross-reference.
    //
    // Each entry is a literal out.push_back({...}) block (no lambda helper)
    // so tools/check_schema_fidelity.py's CORE_BLOCK_RE recognizes them.
    out.push_back({
        "pcsx2_audio_sync_mode",
        "Audio Sync Mode",
        nullptr,
        "How emulated audio is paced against host audio. TimeStretch "
        "resamples audio (via SoundTouch) so pitch stays correct when "
        "emulation speed differs from 100%. Disabled passes raw samples "
        "through — fastest but produces audible artefacts during any "
        "speed deviation.",
        nullptr,
        nullptr,
        {
            { "Disabled",    "Disabled (Noisy)" },
            { "TimeStretch", "TimeStretch (Recommended)" },
            { nullptr,       nullptr },
        },
        "TimeStretch",
    });

    out.push_back({
        "pcsx2_audio_buffer_ms",
        "Audio Buffer Size",
        nullptr,
        "Ring-buffer size for emulated audio in milliseconds. Smaller "
        "values reduce audio latency at the cost of higher CPU pressure "
        "and a greater chance of underruns (crackling). 50 ms is the "
        "PCSX2 default; 30 ms is a reasonable low-latency target on a "
        "machine that can sustain it.",
        nullptr,
        nullptr,
        {
            { "10",  "10 ms (lowest latency)" },
            { "20",  "20 ms" },
            { "30",  "30 ms" },
            { "50",  "50 ms (default)" },
            { "75",  "75 ms" },
            { "100", "100 ms" },
            { "150", "150 ms" },
            { "200", "200 ms" },
            { nullptr, nullptr },
        },
        "50",
    });

    out.push_back({
        "pcsx2_audio_volume",
        "Volume",
        nullptr,
        "Normal-play audio volume as a percentage. 100% is the PS2's "
        "native output level. Values above 100% boost the signal "
        "digitally (may clip on loud passages).",
        nullptr,
        nullptr,
        {
            { "0",   "0% (Muted)" },
            { "25",  "25%" },
            { "50",  "50%" },
            { "75",  "75%" },
            { "100", "100% (default)" },
            { "125", "125%" },
            { "150", "150%" },
            { "175", "175%" },
            { "200", "200% (max)" },
            { nullptr, nullptr },
        },
        "100",
    });

    out.push_back({
        "pcsx2_audio_ff_volume",
        "Fast-Forward Volume",
        nullptr,
        "Volume during fast-forward as a percentage. Independent from "
        "the normal-play volume — useful for muting audio entirely "
        "during fast-forward without affecting regular playback.",
        nullptr,
        nullptr,
        {
            { "0",   "0% (Muted)" },
            { "25",  "25%" },
            { "50",  "50%" },
            { "75",  "75%" },
            { "100", "100% (default)" },
            { "125", "125%" },
            { "150", "150%" },
            { "175", "175%" },
            { "200", "200% (max)" },
            { nullptr, nullptr },
        },
        "100",
    });

    out.push_back({
        "pcsx2_audio_muted",
        "Mute Audio",
        nullptr,
        "Mute all PS2 audio output. RetroNest's UI sounds and any other "
        "non-PS2 audio sources are unaffected.",
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

    // SyncMode: validate against the two PCSX2-accepted strings.
    // Anything else (e.g. an older "ASync" value left in a stale
    // options.json) falls back to the struct default and WARNs —
    // PCSX2's ParseSyncMode would otherwise reject the value and the
    // INI write would be a no-op.
    if (const char* v = query("pcsx2_audio_sync_mode")) {
        if (std::strcmp(v, "Disabled") == 0 || std::strcmp(v, "TimeStretch") == 0) {
            out.sync_mode = v;
        } else {
            FrontendLog(RETRO_LOG_WARN,
                "[CoreOptions] Unknown audio sync_mode '%s'; defaulting to TimeStretch", v);
            out.sync_mode = "TimeStretch";
        }
    }

    // Numeric knobs use the same parse_int helper pattern as Phase 1's
    // ee_cycle_rate / vsync_queue_size — strtol with fallback to the
    // struct default on unparseable input.
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

    parse_int("pcsx2_audio_buffer_ms", out.buffer_ms, 50);
    parse_int("pcsx2_audio_volume",    out.volume,    100);
    parse_int("pcsx2_audio_ff_volume", out.ff_volume, 100);

    if (const char* v = query("pcsx2_audio_muted"))
        out.muted = (std::strcmp(v, "enabled") == 0);
}

#ifndef CORE_OPTIONS_TEST_ONLY
void ApplyDefaults(MemorySettingsInterface& si, const Values& v)
{
    // All 5 keys live under PCSX2's SPU2/Output INI section.
    // SetStringValue routes through MemorySettingsInterface, which is
    // PCSX2's base settings layer (set up by Settings::InitializeDefaults).
    // Pcsx2Config::SPU2Options::LoadSave (Pcsx2Config.cpp:1258-1267) reads
    // these via SettingsWrapEntry / SettingsWrapParsedEnum at VMManager
    // refresh time — LoadStartupSettings() runs after this in
    // InitializeDefaults so the new values take effect on this launch.
    si.SetStringValue("SPU2/Output", "SyncMode", v.sync_mode.c_str());
    si.SetIntValue   ("SPU2/Output", "BufferMS",          v.buffer_ms);
    si.SetIntValue   ("SPU2/Output", "StandardVolume",    v.volume);
    si.SetIntValue   ("SPU2/Output", "FastForwardVolume", v.ff_volume);
    si.SetBoolValue  ("SPU2/Output", "OutputMuted",       v.muted);
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::Audio
