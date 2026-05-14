// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for Pcsx2Libretro::CoreOptions.
// Not built as part of pcsx2_libretro target — manual compile.
//
//   cd pcsx2-libretro/tools
//   clang++ -std=c++20 -I.. test_core_options.cpp \
//       ../CoreOptions.cpp ../CoreOptionsEmulation.cpp ../CoreOptionsAudio.cpp \
//       ../CoreOptionsMemoryCards.cpp ../CoreOptionsGraphics.cpp \
//       -DCORE_OPTIONS_TEST_ONLY -o test_core_options
//   ./test_core_options
//
// CORE_OPTIONS_TEST_ONLY gates each CoreOptions*.cpp's FrontendLog and
// MemorySettingsInterface dependencies so the test links without the
// rest of pcsx2-libretro.

#include "../CoreOptions.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using Pcsx2Libretro::CoreOptions::ReadResolved;
using Pcsx2Libretro::CoreOptions::EmitCoreOptionsV2;
using Pcsx2Libretro::CoreOptions::Resolved;
using Pcsx2Libretro::CoreOptions::BuildDefinitions;

// ---------- Fake env_cb plumbing ----------
//
// libretro's environ_cb is a free C function pointer; we cannot pass
// captures into it. Use file-scope state and a regular function.

namespace fake {
    std::map<std::string, std::string> variables;  // GET_VARIABLE responses
    bool null_for_key = false;                     // if true, return NULL for variables[key]
    std::string null_key;
    bool emit_seen = false;                        // SET_CORE_OPTIONS_V2 was called
    std::vector<std::string> emitted_keys;         // captured for emit-test
    bool emit_returns = true;                      // what the host returns from SET_CORE_OPTIONS_V2

    void reset() {
        variables.clear();
        null_for_key = false;
        null_key.clear();
        emit_seen = false;
        emitted_keys.clear();
        emit_returns = true;
    }
}

static bool fake_env_cb(unsigned cmd, void* data)
{
    if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE) {
        auto* v = static_cast<retro_variable*>(data);
        if (!v || !v->key) return false;
        if (fake::null_for_key && fake::null_key == v->key) {
            v->value = nullptr;
            return true;
        }
        auto it = fake::variables.find(v->key);
        if (it == fake::variables.end()) {
            v->value = nullptr;
            return false;
        }
        v->value = it->second.c_str();
        return true;
    }
    if (cmd == RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2) {
        fake::emit_seen = true;
        auto* opts = static_cast<retro_core_options_v2*>(data);
        if (opts && opts->definitions) {
            for (auto* d = opts->definitions; d->key != nullptr; ++d)
                fake::emitted_keys.emplace_back(d->key);
        }
        return fake::emit_returns;
    }
    return false;
}

static int failures = 0;
static void check_int(const char* label, int got, int want)
{
    const bool ok = (got == want);
    std::printf("[%s] %s: got=%d want=%d\n", ok ? "PASS" : "FAIL", label, got, want);
    if (!ok) ++failures;
}
static void check_bool(const char* label, bool got, bool want)
{
    const bool ok = (got == want);
    std::printf("[%s] %s: got=%s want=%s\n", ok ? "PASS" : "FAIL", label,
                got ? "true" : "false", want ? "true" : "false");
    if (!ok) ++failures;
}

int main()
{
    // -------- Case 1: happy path — all three keys present, non-default --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "metal";
    fake::variables["pcsx2_mtvu"]      = "disabled";
    fake::variables["pcsx2_fast_boot"] = "disabled";

    Resolved r = ReadResolved(&fake_env_cb);
    check_int ("Case 1 renderer",  r.emulation.renderer,  17);
    check_bool("Case 1 mtvu",      r.emulation.mtvu,      false);
    check_bool("Case 1 fast_boot", r.emulation.fast_boot, false);

    // -------- Case 2: NULL value for one key — that field stays at default --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "software";  // 13
    fake::null_for_key = true;
    fake::null_key = "pcsx2_mtvu";                    // → default true
    fake::variables["pcsx2_fast_boot"] = "disabled";  // → false

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 2 renderer",  r.emulation.renderer,  13);
    check_bool("Case 2 mtvu",      r.emulation.mtvu,      true);   // default
    check_bool("Case 2 fast_boot", r.emulation.fast_boot, false);

    // -------- Case 3: unknown renderer enum → default -1 (Auto) --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "vulkan";        // not in our schema
    fake::variables["pcsx2_mtvu"]      = "enabled";
    fake::variables["pcsx2_fast_boot"] = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 3 renderer (unknown)", r.emulation.renderer, -1);
    check_bool("Case 3 mtvu",               r.emulation.mtvu,      true);
    check_bool("Case 3 fast_boot",          r.emulation.fast_boot, true);

    // -------- Case 4: all defaults — every key returns its declared default string --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "auto";
    fake::variables["pcsx2_mtvu"]      = "enabled";
    fake::variables["pcsx2_fast_boot"] = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 4 renderer",  r.emulation.renderer,  -1);
    check_bool("Case 4 mtvu",      r.emulation.mtvu,      true);
    check_bool("Case 4 fast_boot", r.emulation.fast_boot, true);

    // -------- Case 5: EmitCoreOptionsV2 dispatches the SP7b head triplet --------
    //
    // SP7c Phase 1+ appends more knobs, so the total count grows over time.
    // We assert >= 3 (sentinel) and pin the first three positions to the
    // SP7b order — same regression-sentinel pattern Case 6 adopted in
    // Task 1. Per-entry shape is covered by Case 7.
    fake::reset();
    fake::emit_returns = true;
    const bool emit_ok = EmitCoreOptionsV2(&fake_env_cb);
    check_bool("Case 5 emit returned true", emit_ok, true);
    check_int ("Case 5 emit was seen",
               static_cast<int>(fake::emit_seen ? 1 : 0), 1);
    check_bool("Case 5 emitted >= 3 keys",
               fake::emitted_keys.size() >= 3, true);
    // Order matches the kDefinitions[] table order in CoreOptions.cpp.
    auto str_eq = [](const std::string& a, const char* b) { return a == b; };
    check_bool("Case 5 key 0 = pcsx2_renderer",
               !fake::emitted_keys.empty()
               && str_eq(fake::emitted_keys[0], "pcsx2_renderer"), true);
    check_bool("Case 5 key 1 = pcsx2_mtvu",
               fake::emitted_keys.size() > 1
               && str_eq(fake::emitted_keys[1], "pcsx2_mtvu"), true);
    check_bool("Case 5 key 2 = pcsx2_fast_boot",
               fake::emitted_keys.size() > 2
               && str_eq(fake::emitted_keys[2], "pcsx2_fast_boot"), true);

    // -------- Case 6: BuildDefinitions retains SP7b's 3 keys at the head, terminator at the tail --------
    //
    // SP7c Phase 1+ adds more knobs; this case is a regression sentinel,
    // not a count fixture. Per-knob sanity is covered by Case 7's data-driven
    // sweep. Three invariants:
    //   1. The first 3 keys are still pcsx2_renderer / pcsx2_mtvu / pcsx2_fast_boot
    //      in that order (SP7b call-site ordering must not regress).
    //   2. Total entries strictly greater than 4 once Phase 1 lands (>= 19 after
    //      all 15 Phase-1 knobs); Phase 0 leaves it at 4. We assert >= 4 here
    //      and let Case 7 catch sub-entry shape.
    //   3. The final entry is the libretro terminator (key == nullptr).
    {
        const auto& defs = BuildDefinitions();
        check_bool("Case 6 size >= 4 (SP7b minimum)",
                   defs.size() >= 4, true);

        if (defs.size() >= 4) {
            check_bool("Case 6 [0].key = pcsx2_renderer",
                       defs[0].key && std::strcmp(defs[0].key, "pcsx2_renderer") == 0, true);
            check_bool("Case 6 [1].key = pcsx2_mtvu",
                       defs[1].key && std::strcmp(defs[1].key, "pcsx2_mtvu") == 0, true);
            check_bool("Case 6 [2].key = pcsx2_fast_boot",
                       defs[2].key && std::strcmp(defs[2].key, "pcsx2_fast_boot") == 0, true);
            const auto& last = defs.back();
            check_bool("Case 6 last entry is terminator",
                       last.key == nullptr, true);
        }
    }

    // -------- Case 7: structural sanity for every definition --------
    //
    // For every non-terminator definition:
    //   - key is non-NULL
    //   - desc is non-NULL
    //   - default_value is non-NULL and appears in values[]
    //   - values[] has at least one non-terminator entry
    //   - values[] is itself terminated by {nullptr, nullptr}
    //
    // This catches accidental missing fields when adding new options.
    {
        const auto& defs = BuildDefinitions();
        std::map<std::string, int> key_counts;

        for (size_t i = 0; i + 1 < defs.size(); ++i) {
            const auto& d = defs[i];
            check_bool("Case 7 key non-null",   d.key != nullptr, true);
            check_bool("Case 7 desc non-null",  d.desc != nullptr, true);
            check_bool("Case 7 default non-null", d.default_value != nullptr, true);

            // Look up default in values[].
            bool default_in_values = false;
            int values_count = 0;
            for (const auto& vp : d.values) {
                if (vp.value == nullptr) break;
                ++values_count;
                if (d.default_value && std::strcmp(vp.value, d.default_value) == 0)
                    default_in_values = true;
            }
            check_bool("Case 7 values has at least 1 entry", values_count >= 1, true);
            check_bool("Case 7 default appears in values", default_in_values, true);

            if (d.key)
                ++key_counts[d.key];
        }

        // No duplicate keys.
        for (const auto& [k, c] : key_counts) {
            check_int(("Case 7 unique key " + k).c_str(), c, 1);
        }

        // The last entry must be the terminator.
        if (!defs.empty()) {
            const auto& last = defs.back();
            check_bool("Case 7 last entry is terminator", last.key == nullptr, true);
        }
    }

    // -------- Case 8: Speed Control round-trip --------
    //
    // SP7c Phase 1 representative test for the Speed Control sub-group.
    // We pick one knob per sub-group (Case 8/9/10) rather than testing all
    // 15 individually — Case 7's structural sweep already proves every
    // entry has well-formed values/default; this case proves the parse
    // path's float conversion works end-to-end.
    fake::reset();
    fake::variables["pcsx2_renderer"]            = "auto";
    fake::variables["pcsx2_mtvu"]                = "enabled";
    fake::variables["pcsx2_fast_boot"]           = "enabled";
    fake::variables["pcsx2_normal_speed"]        = "1.5";
    fake::variables["pcsx2_fast_forward_speed"]  = "4";
    fake::variables["pcsx2_slow_motion_speed"]   = "0.25";

    r = ReadResolved(&fake_env_cb);
    {
        // Float comparisons: literal "1.5" → 1.5f exactly under IEEE 754.
        const bool ns_ok  = r.emulation.normal_speed       == 1.5f;
        const bool ffs_ok = r.emulation.fast_forward_speed == 4.0f;
        const bool sms_ok = r.emulation.slow_motion_speed  == 0.25f;
        check_bool("Case 8 normal_speed=1.5",        ns_ok,  true);
        check_bool("Case 8 fast_forward_speed=4",    ffs_ok, true);
        check_bool("Case 8 slow_motion_speed=0.25",  sms_ok, true);
    }

    // Unparseable string falls back to default 1.0
    fake::reset();
    fake::variables["pcsx2_normal_speed"] = "not-a-number";
    r = ReadResolved(&fake_env_cb);
    check_bool("Case 8 garbled normal_speed → default 1.0",
               r.emulation.normal_speed == 1.0f, true);

    // -------- Case 9: System Settings round-trip --------
    fake::reset();
    fake::variables["pcsx2_renderer"]      = "auto";
    fake::variables["pcsx2_mtvu"]          = "enabled";
    fake::variables["pcsx2_fast_boot"]     = "enabled";
    fake::variables["pcsx2_ee_cycle_rate"] = "-1";
    fake::variables["pcsx2_ee_cycle_skip"] = "2";
    fake::variables["pcsx2_thread_pinning"]= "enabled";
    fake::variables["pcsx2_cheats"]        = "enabled";
    fake::variables["pcsx2_host_fs"]       = "disabled";
    fake::variables["pcsx2_cdvd_precache"] = "enabled";
    fake::variables["pcsx2_fast_boot_ff"]  = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 9 ee_cycle_rate=-1",  r.emulation.ee_cycle_rate,  -1);
    check_int ("Case 9 ee_cycle_skip=2",   r.emulation.ee_cycle_skip,   2);
    check_bool("Case 9 thread_pinning",    r.emulation.thread_pinning,  true);
    check_bool("Case 9 cheats",            r.emulation.cheats,          true);
    check_bool("Case 9 host_fs=off",       r.emulation.host_fs,         false);
    check_bool("Case 9 cdvd_precache",     r.emulation.cdvd_precache,   true);
    check_bool("Case 9 fast_boot_ff",      r.emulation.fast_boot_ff,    true);

    // -------- Case 10: Frame Pacing round-trip --------
    fake::reset();
    fake::variables["pcsx2_renderer"]               = "auto";
    fake::variables["pcsx2_mtvu"]                   = "enabled";
    fake::variables["pcsx2_fast_boot"]              = "enabled";
    fake::variables["pcsx2_vsync_queue_size"]       = "0";
    fake::variables["pcsx2_sync_to_host_rr"]        = "enabled";
    fake::variables["pcsx2_vsync"]                  = "enabled";
    fake::variables["pcsx2_use_vsync_timing"]       = "enabled";
    fake::variables["pcsx2_skip_duplicate_frames"]  = "disabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 10 vsync_queue_size=0",  r.emulation.vsync_queue_size,      0);
    check_bool("Case 10 sync_to_host_rr",     r.emulation.sync_to_host_rr,       true);
    check_bool("Case 10 vsync",               r.emulation.vsync,                 true);
    check_bool("Case 10 use_vsync_timing",    r.emulation.use_vsync_timing,      true);
    check_bool("Case 10 skip_duplicate=off",  r.emulation.skip_duplicate_frames, false);

    // -------- Case 10b: parse_int fallback (garbled input → struct default) --------
    fake::reset();
    fake::variables["pcsx2_ee_cycle_rate"]   = "not-a-number";
    fake::variables["pcsx2_vsync_queue_size"]= "also-garbage";

    r = ReadResolved(&fake_env_cb);
    check_int("Case 10b garbled ee_cycle_rate → default 0",  r.emulation.ee_cycle_rate,      0);
    check_int("Case 10b garbled vsync_queue_size → default 2", r.emulation.vsync_queue_size, 2);

    // -------- Case 11: Audio round-trip --------
    //
    // SP7c Phase 2 representative test for the Audio card. Sets all 5
    // Audio knobs to non-defaults, parses, asserts each field reflects
    // the env-var value. Mirrors Case 9 / Case 10's pattern.
    fake::reset();
    fake::variables["pcsx2_audio_sync_mode"] = "Disabled";
    fake::variables["pcsx2_audio_buffer_ms"] = "30";
    fake::variables["pcsx2_audio_volume"]    = "75";
    fake::variables["pcsx2_audio_ff_volume"] = "0";
    fake::variables["pcsx2_audio_muted"]     = "enabled";

    r = ReadResolved(&fake_env_cb);
    {
        const bool sm_ok = r.audio.sync_mode == "Disabled";
        check_bool("Case 11 sync_mode=Disabled", sm_ok, true);
    }
    check_int ("Case 11 buffer_ms=30",   r.audio.buffer_ms, 30);
    check_int ("Case 11 volume=75",      r.audio.volume,    75);
    check_int ("Case 11 ff_volume=0",    r.audio.ff_volume, 0);
    check_bool("Case 11 muted=on",       r.audio.muted,     true);

    // Unknown sync_mode string falls back to default "TimeStretch" + WARN.
    // Captures a regression scenario: a stale options.json from an
    // older core build with a value that PCSX2's ParseSyncMode would
    // reject. Without the explicit validation in Audio::Parse, the
    // string would flow into g_si unchanged and PCSX2's SettingsWrap
    // would silently reject it at SPU2 load time (hard to debug from
    // an empty log).
    fake::reset();
    fake::variables["pcsx2_audio_sync_mode"] = "ASync";
    r = ReadResolved(&fake_env_cb);
    {
        const bool sm_default = r.audio.sync_mode == "TimeStretch";
        check_bool("Case 11 unknown sync_mode → TimeStretch", sm_default, true);
    }

    // -------- Case 12: Memory Cards round-trip --------
    //
    // SP7c Phase 3 representative test for the Memory Cards card. Sets
    // all 5 MemoryCards knobs to NON-default values (slot1=off,
    // slot2=off, all 3 multitap=on), parses, asserts each field reflects
    // the env-var value. The non-default choice for each knob means a
    // broken Parse branch (e.g. struct-init bleed-through) would be
    // caught — testing slot1=on against the default true would be a
    // tautology.
    fake::reset();
    fake::variables["pcsx2_mc_slot1_enable"]    = "disabled";
    fake::variables["pcsx2_mc_slot2_enable"]    = "disabled";
    fake::variables["pcsx2_mc_multitap1_slot2"] = "enabled";
    fake::variables["pcsx2_mc_multitap1_slot3"] = "enabled";
    fake::variables["pcsx2_mc_multitap1_slot4"] = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_bool("Case 12 slot1=disabled",    r.memory_cards.slot1_enable,    false);
    check_bool("Case 12 slot2=disabled",    r.memory_cards.slot2_enable,    false);
    check_bool("Case 12 mt1_slot2=enabled", r.memory_cards.multitap1_slot2, true);
    check_bool("Case 12 mt1_slot3=enabled", r.memory_cards.multitap1_slot3, true);
    check_bool("Case 12 mt1_slot4=enabled", r.memory_cards.multitap1_slot4, true);

    // -------- Case 12b: Memory Cards default-when-unset --------
    //
    // Confirms that when the host returns NULL for every Memory Cards
    // key (the "user has never opened the dialog" case), Resolved's
    // struct defaults stand: both slots enabled, all 3 multitap slots
    // disabled. This is the parity-with-standalone behavior introduced
    // in Phase 3 — pre-Phase-3 had Slot2 default = false.
    fake::reset();
    // Intentionally do not set any pcsx2_mc_* keys.

    r = ReadResolved(&fake_env_cb);
    check_bool("Case 12b slot1 default=on",   r.memory_cards.slot1_enable,    true);
    check_bool("Case 12b slot2 default=on",   r.memory_cards.slot2_enable,    true);
    check_bool("Case 12b mt1_s2 default=off", r.memory_cards.multitap1_slot2, false);
    check_bool("Case 12b mt1_s3 default=off", r.memory_cards.multitap1_slot3, false);
    check_bool("Case 12b mt1_s4 default=off", r.memory_cards.multitap1_slot4, false);

    // -------- Case 13: Graphics/Display round-trip --------
    //
    // SP7c Phase 4 Task 2 representative test for the Display sub-tab.
    // Picks one knob from each of the 3 value-encoding flavors:
    //   - string-valued combo (aspect_ratio: "16:9")
    //   - int-valued combo (stretch_y: 150 — an enumerated stop)
    //   - bool toggled away from its struct default (pcrtc_antiblur:
    //     disabled, since the default is true — flipping to false
    //     proves Parse runs vs. silent default bleed-through)
    fake::reset();
    fake::variables["pcsx2_aspect_ratio"]   = "16:9";
    fake::variables["pcsx2_stretch_y"]      = "150";
    fake::variables["pcsx2_pcrtc_antiblur"] = "disabled";

    r = ReadResolved(&fake_env_cb);
    {
        const bool ar_ok = r.graphics.display.aspect_ratio == "16:9";
        check_bool("Case 13 aspect_ratio=16:9", ar_ok, true);
    }
    check_int ("Case 13 stretch_y=150",       r.graphics.display.stretch_y, 150);
    check_bool("Case 13 pcrtc_antiblur=off",  r.graphics.display.pcrtc_antiblur, false);

    // -------- Case 13b: Graphics/Display default-when-unset --------
    //
    // Confirms struct defaults match standalone's per-row defaults when
    // no env-var is set. Spot-checks one of each type. The pcrtc_antiblur
    // default-true case is the critical one — it would catch a "missing
    // = true initializer" regression in Values::Display.
    fake::reset();

    r = ReadResolved(&fake_env_cb);
    {
        const bool ar_default = r.graphics.display.aspect_ratio == "4:3";
        check_bool("Case 13b aspect_ratio default=4:3", ar_default, true);
    }
    check_int ("Case 13b stretch_y default=100",
                r.graphics.display.stretch_y, 100);
    check_bool("Case 13b pcrtc_antiblur default=on",
                r.graphics.display.pcrtc_antiblur, true);

    // -------- Case 14: Graphics/Rendering round-trip --------
    //
    // SP7c Phase 4 Task 3 representative test for the Rendering sub-tab.
    // Picks knobs across the value-encoding flavors:
    //   - int-valued combo (upscale_multiplier: 4 — an enumerated stop)
    //   - int-valued combo with dependsOn (filter: 3)
    //   - bool toggled away from struct default (hw_mipmap: disabled,
    //     since the default is true — flipping to false proves Parse
    //     runs vs. silent default bleed-through)
    fake::reset();
    fake::variables["pcsx2_upscale_multiplier"] = "4";
    fake::variables["pcsx2_filter"]             = "3";
    fake::variables["pcsx2_hw_mipmap"]          = "disabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 14 upscale_multiplier=4", r.graphics.rendering.upscale_multiplier, 4);
    check_int ("Case 14 filter=3",             r.graphics.rendering.filter,             3);
    check_bool("Case 14 hw_mipmap=off",        r.graphics.rendering.hw_mipmap,          false);

    // -------- Case 14b: Graphics/Rendering default-when-unset --------
    //
    // Confirms struct defaults match standalone's per-row defaults when
    // no env-var is set. The hw_mipmap default-true case is the critical
    // one — it would catch a "missing = true initializer" regression in
    // Values::Rendering. tri_filter=-1 catches negative-default parsing.
    fake::reset();

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 14b upscale_multiplier default=1",
                r.graphics.rendering.upscale_multiplier, 1);
    check_int ("Case 14b filter default=2",
                r.graphics.rendering.filter, 2);
    check_int ("Case 14b tri_filter default=-1",
                r.graphics.rendering.tri_filter, -1);
    check_bool("Case 14b hw_mipmap default=on",
                r.graphics.rendering.hw_mipmap, true);

    // -------- Case 15: Graphics/Texture Replacement round-trip --------
    //
    // SP7c Phase 4 Task 4 representative test for the Texture Replacement
    // sub-tab. All 6 knobs are bool, so we cover:
    //   - bool toggled to true on a default-false row (load=enabled
    //     proves Parse runs vs. silent default-false bleed-through)
    //   - bool toggled to false on the only default-true row
    //     (load_texture_replacements_async — would catch a missing
    //     "= true" initializer in Values::TextureReplacement)
    fake::reset();
    fake::variables["pcsx2_load_texture_replacements"]       = "enabled";
    fake::variables["pcsx2_load_texture_replacements_async"] = "disabled";

    r = ReadResolved(&fake_env_cb);
    check_bool("Case 15 load_texture_replacements=on",
                r.graphics.texture_replacement.load_texture_replacements, true);
    check_bool("Case 15 load_texture_replacements_async=off",
                r.graphics.texture_replacement.load_texture_replacements_async, false);

    // -------- Case 15b: Graphics/Texture Replacement default-when-unset
    //
    // Confirms struct defaults match standalone. The async default-true
    // case is the critical one — same shape as Case 14b's hw_mipmap.
    fake::reset();

    r = ReadResolved(&fake_env_cb);
    check_bool("Case 15b load_texture_replacements default=off",
                r.graphics.texture_replacement.load_texture_replacements, false);
    check_bool("Case 15b load_texture_replacements_async default=on",
                r.graphics.texture_replacement.load_texture_replacements_async, true);
    check_bool("Case 15b dump_replaceable_textures default=off",
                r.graphics.texture_replacement.dump_replaceable_textures, false);
    check_bool("Case 15b precache_texture_replacements default=off",
                r.graphics.texture_replacement.precache_texture_replacements, false);
    check_bool("Case 15b dump_replaceable_mipmaps default=off",
                r.graphics.texture_replacement.dump_replaceable_mipmaps, false);
    check_bool("Case 15b dump_textures_with_fmv_active default=off",
                r.graphics.texture_replacement.dump_textures_with_fmv_active, false);

    // -------- Case 16: Graphics/Post-Processing round-trip --------
    //
    // SP7c Phase 4 Task 5 representative test for the Post-Processing
    // sub-tab. Picks one knob per value-encoding flavor + one per group:
    //   - 3-stop Combo, non-default (cas_mode = 1)
    //   - 5-stop Combo, non-default, depends on cas_mode flip
    //     (cas_sharpness = 75)
    //   - bool toggled to true (fxaa = enabled)
    //   - 8-stop Combo (tv_shader = 3)
    //   - bool master toggled to true (shade_boost = enabled)
    //   - 5-stop Combo (shade_boost_brightness = 25)
    //   - 5-stop Combo at edge stop (shade_boost_gamma = 0)
    // Leaves shade_boost_contrast and shade_boost_saturation unset so
    // ReadResolved skips their Parse branches — the assertions below
    // confirm those fields hold their struct-default value of 50, which
    // catches a missing initializer in Values::PostProcessing.
    fake::reset();
    fake::variables["pcsx2_cas_mode"]               = "1";
    fake::variables["pcsx2_cas_sharpness"]          = "75";
    fake::variables["pcsx2_fxaa"]                   = "enabled";
    fake::variables["pcsx2_tv_shader"]              = "3";
    fake::variables["pcsx2_shade_boost"]            = "enabled";
    fake::variables["pcsx2_shade_boost_brightness"] = "25";
    fake::variables["pcsx2_shade_boost_gamma"]      = "0";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 16 cas_mode=1",
                r.graphics.post_processing.cas_mode, 1);
    check_int ("Case 16 cas_sharpness=75",
                r.graphics.post_processing.cas_sharpness, 75);
    check_bool("Case 16 fxaa=on",
                r.graphics.post_processing.fxaa, true);
    check_int ("Case 16 tv_shader=3",
                r.graphics.post_processing.tv_shader, 3);
    check_bool("Case 16 shade_boost=on",
                r.graphics.post_processing.shade_boost, true);
    check_int ("Case 16 shade_boost_brightness=25",
                r.graphics.post_processing.shade_boost_brightness, 25);
    check_int ("Case 16 shade_boost_contrast struct-default=50",
                r.graphics.post_processing.shade_boost_contrast, 50);
    check_int ("Case 16 shade_boost_saturation struct-default=50",
                r.graphics.post_processing.shade_boost_saturation, 50);
    check_int ("Case 16 shade_boost_gamma=0 (edge)",
                r.graphics.post_processing.shade_boost_gamma, 0);

    // -------- Case 16b: Graphics/Post-Processing default-when-unset --------
    //
    // Confirms the full default vector when no env-var is set. There is
    // no single "non-default-default" anchor in this sub-tab (all 5
    // sliders default to 50, all 2 bools to false, all 2 combos to 0)
    // — so we assert all 9 fields explicitly to catch drift in any
    // individual initializer.
    fake::reset();

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 16b cas_mode default=0",
                r.graphics.post_processing.cas_mode, 0);
    check_int ("Case 16b cas_sharpness default=50",
                r.graphics.post_processing.cas_sharpness, 50);
    check_bool("Case 16b fxaa default=off",
                r.graphics.post_processing.fxaa, false);
    check_int ("Case 16b tv_shader default=0",
                r.graphics.post_processing.tv_shader, 0);
    check_bool("Case 16b shade_boost default=off",
                r.graphics.post_processing.shade_boost, false);
    check_int ("Case 16b shade_boost_brightness default=50",
                r.graphics.post_processing.shade_boost_brightness, 50);
    check_int ("Case 16b shade_boost_contrast default=50",
                r.graphics.post_processing.shade_boost_contrast, 50);
    check_int ("Case 16b shade_boost_saturation default=50",
                r.graphics.post_processing.shade_boost_saturation, 50);
    check_int ("Case 16b shade_boost_gamma default=50",
                r.graphics.post_processing.shade_boost_gamma, 50);

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
